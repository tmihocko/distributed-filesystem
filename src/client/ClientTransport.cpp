#include "ClientTransport.hpp"
#include "asio/post.hpp"
#include "network/Message.hpp"
#include "protocol/ClientProtocol.hpp"
#include <cerrno>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <vector>
#include "network/FrameIO.hpp"

namespace CP = ClientProtocol;

constexpr auto CONNECT_TIMEOUT = std::chrono::seconds{ 5 };
constexpr auto REQUEST_TIMEOUT = std::chrono::seconds{ 10 };

std::uint64_t make_session_id() {
	const auto now =
		std::chrono::steady_clock::now().time_since_epoch().count();

	return static_cast<std::uint64_t>(now);
}

ClientTransport::ClientTransport() : session_id_(make_session_id()), io_thread_([this] {
										 context_.run();
									 }) {}

// more like connect_to_cluster
TransportOperation<void> ClientTransport::connect(std::span<const Endpoint> seed_nodes) {
	if (seed_nodes.empty()) {
		return std::unexpected(TransportError{
			TransportErrorCode::NO_SEEDS,
			"Seed nodes is empty." });
	}

	if (shutting_down_) {
		return std::unexpected(TransportError{
			TransportErrorCode::SHUTTING_DOWN,
			"The client transport has been shut down." });
	}

	seed_nodes_.assign(seed_nodes.begin(), seed_nodes.end());
	current_leader_.reset();

	return discover_leader();
}

// Ensures that the transport has a TCP connection to the supplied endpoint.
//
// If the current socket already targets that endpoint, it is reused.
// Otherwise, the previous socket is closed, the host is resolved, and an
// asynchronous TCP connection is started on the Asio thread. This method
// blocks the client thread until connection succeeds, fails, or times out.
//
// This method only establishes a TCP connection. It does not prove that the
// endpoint is the Raft leader; request() handles leader redirection later.
TransportOperation<void> ClientTransport::connect_to(const Endpoint &endpoint) {
	if (endpoint.host.empty() || endpoint.port == 0) {
		return std::unexpected(TransportError{
			TransportErrorCode::CONNECT_FAILED,
			"Endpoint requires a host and non-zero port." });
	}

	if (connected_endpoint_ &&
		(connected_endpoint_->host == endpoint.host &&
		 connected_endpoint_->port == endpoint.port)) {
		return {};
	}

	connected_endpoint_.reset();

	const auto op_id = next_io_operation_id_++;

	auto cancelled = std::make_shared<std::atomic_bool>(false);

	asio::post(context_, [this, endpoint, op_id, cancelled]() {
		if (shutting_down_ || *cancelled) {
			connect_results_.push(ConnectResult{
				.operation_id = op_id,
				.error = std::make_error_code(std::errc::operation_canceled),
				.endpoint = endpoint,
			});
			return;
		}

		close_socket();

		resolver_.async_resolve(
			endpoint.host,
			std::to_string(endpoint.port),
			[this, endpoint, op_id, cancelled](const asio::error_code &resolve_error, asio::ip::tcp::resolver::results_type results) {
				if (*cancelled) return;

				if (resolve_error) {
					connect_results_.push(ConnectResult{
						.operation_id = op_id,
						.error = resolve_error,
						.endpoint = endpoint,
					});
					return;
				}

				asio::async_connect(
					socket_,
					results,
					[this, endpoint, op_id, cancelled](const asio::error_code &connect_error, const asio::ip::tcp::endpoint &) {
						if (*cancelled) {
							close_socket();
							return;
						}

						connect_results_.push(ConnectResult{
							.operation_id = op_id,
							.error = connect_error,
							.endpoint = endpoint,
						});
					});
			});
	});

	const auto deadline = std::chrono::steady_clock::now() + CONNECT_TIMEOUT;
	while (true) {
		const auto now = std::chrono::steady_clock::now();

		if (now >= deadline) {
			*cancelled = true;

			asio::post(context_, [this]() {
				resolver_.cancel();
				close_socket();
			});

			return std::unexpected(TransportError(
				TransportErrorCode::TIMEOUT,
				"Timed out connecting to " +
					endpoint.key() + '.'));
		}

		auto result =
			connect_results_.pop(deadline - now);

		if (!result ||
			result->operation_id != op_id) {
			continue;
		}

		if (result->error) {
			return std::unexpected(TransportError(
				TransportErrorCode::CONNECT_FAILED,
				"Could not connect to " +
					endpoint.key() + ": " +
					result->error.message()));
		}

		connected_endpoint_ = endpoint;
		return {};
	}
}

TransportOperation<void> ClientTransport::discover_leader() {
	if (seed_nodes_.empty()) {
		return std::unexpected(TransportError{
			TransportErrorCode::NO_SEEDS,
			"No server endpoint was configured.",
		});
	}

	std::optional<TransportError> last_error;

	for (const auto &seed : seed_nodes_) {
		auto connected = connect_to(seed);

		if (!connected) {
			last_error = std::move(connected.error());
			continue;
		}

		current_leader_ = seed;
		return {};
	}

	if (last_error) {
		return std::unexpected(std::move(*last_error));
	}

	return std::unexpected(TransportError{
		TransportErrorCode::CONNECT_FAILED,
		"Could not connect to any seed node." });
}

TransportOperation<ClientProtocol::RpcResponse> ClientTransport::request(
	CP::Operation operation,
	std::vector<std::byte> payload) {
	if (shutting_down_) {
		return std::unexpected(TransportError(
			TransportErrorCode::SHUTTING_DOWN,
			"The client transport has been shut down."));
	}

	if (!current_leader_) {
		return std::unexpected(TransportError(
			TransportErrorCode::NOT_CONNECTED,
			"Call connect() before request()."));
	}

	CP::RpcRequest request{
		.client_session_id = session_id_,
		.request_id = next_request_id_++,
		.operation = operation,
		.payload = std::move(payload)
	};

	constexpr int MAX_REDIRECTS = 3;

	for (int attempt = 0; attempt <= MAX_REDIRECTS; ++attempt) {

		if (!current_leader_) {
			return std::unexpected(TransportError{
				TransportErrorCode::NOT_CONNECTED,
				"No Raft leader is currently known.",
			});
		}

		const Endpoint leader = *current_leader_;

		auto response = send_and_wait(leader, request);
		if (!response || response->status != CP::RpcStatus::NOT_LEADER) return response;

		if (!response->leader_hint) {
			return std::unexpected(TransportError{
				TransportErrorCode::PROTOCOL_ERROR,
				"Follower returned NOT_LEADER without a leader hint.",
			});
		}

		current_leader_ = *response->leader_hint;
	}

	return std::unexpected(TransportError{
		TransportErrorCode::REDIRECT_LIMIT,
		"Leader changed too many times while sending the request.",
	});
}

TransportOperation<ClientProtocol::RpcResponse> ClientTransport::send_and_wait(
	const Endpoint &leader,
	const CP::RpcRequest &request) {

	auto connected = connect_to(leader);

	if (!connected) return std::unexpected(std::move(connected.error()));

	auto outgoing = CP::encode_request(request);

	if (!outgoing) {
		return std::unexpected(TransportError{
			TransportErrorCode::PROTOCOL_ERROR,
			outgoing.error().what,
		});
	}

	auto incoming = perform_frame_round_trip(std::move(*outgoing));

	if (!incoming) return std::unexpected(std::move(incoming.error()));

	auto response = ClientProtocol::decode_response(*incoming);

	if (!response) {
		connected_endpoint_.reset();
		asio::post(context_, [this]() {
			close_socket();
		});

		return std::unexpected(TransportError{
			TransportErrorCode::PROTOCOL_ERROR,
			response.error().what,
		});
	}

	if (response->request_id != request.request_id) {
		connected_endpoint_.reset();

		asio::post(context_, [this]() {
			close_socket();
		});

		return std::unexpected(TransportError{
			TransportErrorCode::PROTOCOL_ERROR,
			"Response request ID does not match.",
		});
	}

	return std::move(*response);
}

// Sends one encoded frame on the current TCP connection and waits for
// the corresponding response frame.
//
// This method handles asynchronous socket I/O, transports the result from
// the Asio thread back to the client thread, and enforces a response timeout.
// It does not encode or decode RPC messages and assumes the socket is connected.
TransportOperation<Frame> ClientTransport::perform_frame_round_trip(Frame outgoing) {
	const auto op_id = next_io_operation_id_++;

	asio::post(
		context_,
		[this, op_id, outgoing = std::move(outgoing)]() mutable {
			FrameIO::async_write_frame(
				socket_,
				std::move(outgoing),
				[this, op_id](
					const asio::error_code &write_error, std::size_t) {
					if (write_error) {
						read_results_.push(ReadResult{
							.operation_id = op_id,
							.error = write_error,
							.frame = std::nullopt,
						});
						return;
					}

					FrameIO::async_read_frame(
						socket_,
						[this, op_id](
							const asio::error_code &read_error, std::optional<Frame> frame) {
							read_results_.push(ReadResult{
								.operation_id = op_id,
								.error = read_error,
								.frame = std::move(frame),
							});
						});
				});
		});

	const auto deadline = std::chrono::steady_clock::now() + REQUEST_TIMEOUT;

	while (std::chrono::steady_clock::now() < deadline) {
		const auto remaining = deadline - std::chrono::steady_clock::now();

		auto result = read_results_.pop(remaining);

		if (!result) break;

		// Ignore late results from an earlier timed-out request.
		if (result->operation_id != op_id) continue;

		if (result->error || !result->frame) {
			connected_endpoint_.reset();

			asio::post(context_, [this]() {
				close_socket();
			});

			const std::string detail = result->error ? result->error.message() : "No response frame received.";

			return std::unexpected(TransportError{
				TransportErrorCode::IO_ERROR,
				"Request failed: " + detail,
			});
		}

		return std::move(*result->frame);
	}

	connected_endpoint_.reset();

	asio::post(context_, [this]() {
		close_socket();
	});

	return std::unexpected(TransportError{
		TransportErrorCode::TIMEOUT,
		"Timed out waiting for the server response.",
	});
}

void ClientTransport::close_socket() {
	asio::error_code ignored;

	socket_.cancel(ignored);
	socket_.shutdown(
		asio::ip::tcp::socket::shutdown_both,
		ignored);
	socket_.close(ignored);
}

void ClientTransport::shutdown() {
	if (shutting_down_.exchange(true)) return;

	asio::post(context_, [this]() {
		resolver_.cancel();
		close_socket();
	});

	guard_.reset();

	if (io_thread_.joinable()) io_thread_.join();
}

ClientTransport::~ClientTransport() {
	shutdown();
}