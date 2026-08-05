#include "ClientTransport.hpp"
#include "protocol/ClientProtocol.hpp"
#include <chrono>
#include <expected>
#include <vector>

namespace CP = ClientProtocol;

std::uint64_t make_session_id() {
	const auto now =
		std::chrono::steady_clock::now().time_since_epoch().count();

	return static_cast<std::uint64_t>(now);
}

ClientTransport::ClientTransport() : session_id_(make_session_id()), io_thread_([this] {
										 context_.run();
									 }) {}

std::expected<void, TransportError> ClientTransport::connect(std::span<const Endpoint> seed_nodes) {
	if (seed_nodes.empty()) {
		return std::unexpected(TransportError{
			TransportErrorCode::NO_SEEDS,
			"Seed nodes is empty." });
	}

	seed_nodes_.assign(seed_nodes.begin(), seed_nodes.end());
	current_leader_.reset();

	return discover_leader();
}

std::expected<void, TransportError> ClientTransport::discover_leader() {
	if (seed_nodes_.empty()) {
		return std::unexpected(TransportError{
			TransportErrorCode::NO_SEEDS,
			"No server endpoint was configured.",
		});
	}

	current_leader_ = seed_nodes_.front();

	return {};
}

std::expected<CP::RpcResponse, TransportError>
ClientTransport::request(CP::Operation operation, std::vector<std::byte> payload) {

	CP::RpcRequest request{
		.client_session_id = session_id_,
		.request_id = next_request_id_++,
		.operation = operation,
		.payload = std::move(payload)
	};

	constexpr int MAX_REDIRECTS = 3;

	for (int attempt = 0; attempt < MAX_REDIRECTS; ++attempt) {

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

std::expected<CP::RpcResponse, TransportError> ClientTransport::send_and_wait(
	const Endpoint &leader,
	const CP::RpcRequest &request) {

	auto frame = CP::encode_request(request);

	if (!frame) {
		return std::unexpected(TransportError{
			TransportErrorCode::PROTOCOL_ERROR,
			frame.error().what,
		});
	}

	/*
	 * The next step will:
	 *
	 * 1. Ensure socket_ is connected to leader.
	 * 2. Post this frame to the I/O thread.
	 * 3. Wait on read_results_.
	 * 4. Call CP::decode_response().
	 * 5. Validate response.request_id.
	 */

	return std::unexpected(TransportError{
		TransportErrorCode::NOT_IMPLEMENTED,
		"Client frame sending is not implemented yet.",
	});
}

void ClientTransport::shutdown() {
	if (shutting_down_) return;

	shutting_down_ = true;

	asio::post(context_, [this]() {
		asio::error_code ignored;

		socket_.cancel(ignored);
		socket_.close(ignored);
	});

	guard_.reset();

	if (io_thread_.joinable()) io_thread_.join();
}

ClientTransport::~ClientTransport() {
	shutdown();
}