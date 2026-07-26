#include "Network.hpp"
#include "Message.hpp"
#include "Packet.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/address.hpp"
#include "asio/ip/tcp.hpp"
#include <array>
#include <asio.hpp>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

// This is where node_id and connection are "strongly binded"
void Network::start_reading(NodeId node_id, std::shared_ptr<Connection> connection) {
	async_read_frame(
		connection->socket,
		[this, node_id = std::move(node_id), connection = std::move(connection)](asio::error_code error, std::optional<Frame> msg) {
			if (error || !msg) {
				connection->socket.close();

				auto it = connections_.find(node_id);
				if (it != connections_.end() &&
					it->second == connection) {
					connections_.erase(it);
				}
				return;
			}

			message_queue_.push(Message{
				node_id,
				msg->header,
				std::move(msg->buffer),
			});

			if (status_ == NetworkStatus::ACTIVE) {
				start_reading(node_id, connection);
			}
		});
}

void Network::run() {
	if (status_ == NetworkStatus::ACTIVE) return;
	// Read messages and put into message queue

	using asio::ip::tcp;

	tcp::endpoint local{ asio::ip::make_address(self_.endpoint.host), self_.endpoint.port };

	acceptor_.open(local.protocol());
	acceptor_.set_option(tcp::acceptor::reuse_address(true));
	acceptor_.bind(local);
	acceptor_.listen();

	status_ = NetworkStatus::ACTIVE;
	accept_peers();

	for (auto &[id, connection] : connections_) {
		start_reading(id, connection);
	}
	network_thread_ = std::jthread{
		[this]() {
			context_.run();
		}
	};
}

void Network::accept_peers() {
	acceptor_.async_accept([this](const asio::error_code &error, asio::ip::tcp::socket socket) {
		if (!error) {
			auto connection = std::make_shared<Connection>(std::move(socket), false);

			pending_connections_.insert(connection);

			send_hello(connection);
			read_hello(connection);
		}
		if (status_ == NetworkStatus::ACTIVE && error != asio::error::operation_aborted) {
			accept_peers();
		}
	});
}

void Network::find_peers(const std::string &config_file) {
	assert(status_ == NetworkStatus::HANDSHAKING);
	self_ = get_node_info(config_file);
	std::vector<NodeInfo> seed_nodes = get_seed_nodes(config_file);

	peers_.clear();

	for (const auto &peer : seed_nodes) {
		const NodeId &id = peer.endpoint.node_id;
		if (id.empty() || id == self_.endpoint.node_id) continue;

		peers_.insert_or_assign(id, peer.endpoint);
		connect_to_peer(peer.endpoint);
	}
}

void Network::register_connection(std::shared_ptr<Connection> connection, Endpoint peer) {
	const NodeId &id = peer.node_id;
	if ((id.empty() || id == self_.endpoint.node_id) ||
		(connection->expected_peer_id && *connection->expected_peer_id != id)) {

		close_pending(connection);
		return;
	}

	connection->peer_id = id;
	peers_.insert_or_assign(id, std::move(peer));

	auto [it, inserted] = connections_.try_emplace(id, connection);

	if (!inserted) {
		close_pending(connection);
		return;
	}

	pending_connections_.erase(connection);
	start_reading(id, std::move(connection));
}

void Network::connect_to_peer(const Endpoint &peer) {
	using asio::ip::tcp;

	auto resolver = std::make_shared<tcp::resolver>(context_);
	auto connection = std::make_shared<Connection>(context_, true);

	connection->expected_peer_id = peer.node_id;
	pending_connections_.insert(connection);

	resolver->async_resolve(
		peer.host,
		std::to_string(peer.port),
		[this, peer, connection, resolver](const asio::error_code &error, tcp::resolver::results_type results) {
			if (error) {
				pending_connections_.erase(connection);
				schedule_reconnect(peer);
				return;
			}

			asio::async_connect(
				connection->socket,
				results,
				[this, peer, connection](const asio::error_code &error, const tcp::endpoint &) {
					if (error) {
						pending_connections_.erase(connection);
						schedule_reconnect(peer);
						return;
					}

					connection->peer_id = peer.node_id;

					send_hello(connection);
					read_hello(connection);
				});
		});
}

Frame Network::read_frame(asio::ip::tcp::socket &socket) {
	std::array<std::byte, MESSAGE_HEADER_SIZE> header_bytes;

	asio::read(socket, asio::buffer(header_bytes));

	PacketReader reader(header_bytes);

	const auto magic = reader.read<std::uint8_t>();
	const auto payload_size = reader.read<std::uint32_t>();
	const auto message_type = reader.read<MessageType>();

	if (magic != HEADER_MAGIC) throw std::runtime_error("Wrong magic!");
	if (payload_size > MAX_MESSAGE_SIZE) throw std::runtime_error("Message exceeds maximum size");

	std::vector<std::byte> payload(payload_size);

	if (payload_size != 0) {
		asio::read(socket, asio::buffer(payload));
	}

	MessageHeader header = { magic, payload_size, message_type };

	return Frame{
		header,
		std::move(payload)
	};
}

void Network::async_read_frame(
	asio::ip::tcp::socket &socket,
	std::function<void(asio::error_code, std::optional<Frame>)> handler) {
	// stuff
	struct ReadState {
		std::array<std::byte, MESSAGE_HEADER_SIZE> header_bytes{};
		MessageHeader header{};
		std::vector<std::byte> payload;
	};

	// Keep memory alive in asio's async world
	auto state = std::make_shared<ReadState>();

	// Header first,
	asio::async_read(socket,
					 asio::buffer(state->header_bytes),
					 [&socket, state, handler = std::move(handler)](
						 const asio::error_code &error,
						 std::size_t) mutable {
						 if (error) {
							 handler(error, std::nullopt);
							 return;
						 }
						 try {
							 PacketReader reader(state->header_bytes);

							 state->header.magic = reader.read<std::uint8_t>();
							 state->header.length = reader.read<std::uint32_t>();
							 state->header.type = reader.read<MessageType>();

							 if (state->header.magic != HEADER_MAGIC) throw std::runtime_error("Wrong magic");
							 if (state->header.length > MAX_MESSAGE_SIZE) throw std::runtime_error("Message exceeds maximum size");
						 } catch (...) {
							 handler(
								 std::make_error_code(std::errc::protocol_error),
								 std::nullopt);
							 return;
						 }
						 state->payload.resize(state->header.length);

						 if (state->payload.empty()) {
							 handler({},
									 Frame{
										 state->header,
										 std::move(state->payload) });
							 return;
						 }

						 // Then actual message
						 asio::async_read(socket, asio::buffer(state->payload), [state, handler = std::move(handler)](const asio::error_code &error, std::size_t) mutable {
							 if (error) {
								 handler(error, std::nullopt);
								 return;
							 }

							 handler({},
									 Frame{
										 state->header,
										 std::move(state->payload) });
						 });
					 });
}

void async_send_frame(std::shared_ptr<Connection> connection, Frame frame) {
}

std::optional<Message> Network::receive_with_timeout() {
	std::chrono::duration<double, std::ratio<1, 1000>> timeout_;
	return message_queue_.pop(timeout_);
}

void Network::send_hello(std::shared_ptr<Connection> connection) {
	PacketWriter writer;

	writer
		.write_string(self_.endpoint.node_id)
		.write_string(self_.endpoint.host)
		.write<std::uint16_t>(self_.endpoint.port);

	Frame frame{
		MessageHeader{
			HEADER_MAGIC,
			writer.length(),
			MessageType::HELLO },
		std::move(writer.data())
	};

	async_send_frame(connection, std::move(frame));
}

void Network::read_hello(std::shared_ptr<Connection> connection) {
	async_read_frame(
		connection->socket,
		[this, connection](
			asio::error_code error,
			std::optional<Frame> frame) {
			if (error || !frame ||
				frame->header.type != MessageType::HELLO) {
				close_pending(connection);
				return;
			}

			try {
				PacketReader reader{ frame->buffer };

				Endpoint peer = {
					.node_id = reader.read_string(),
					.host = reader.read_string(),
					.port = reader.read<std::uint16_t>()
				};

				register_connection(connection, std::move(peer));
			} catch (...) {
				close_pending(connection);
			}
		});
}
void Network::close_pending(const std::shared_ptr<Connection> &connection) {
	pending_connections_.erase(connection);

	connection->socket.shutdown(asio::ip::tcp::socket::shutdown_both);
	connection->socket.close();
}