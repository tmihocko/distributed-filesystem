#include "Network.hpp"
#include "Message.hpp"
#include "Packet.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/tcp.hpp"
#include "network/FrameIO.hpp"
#include "network/Node.hpp"
#include <asio.hpp>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>

// This is where node_id and connection are "strongly binded"
void Network::start_reading(std::shared_ptr<Connection> connection) {

	if (!connection->remote) {
		close_pending(connection);
		return;
	}

	const NodeIdentity remote = *connection->remote;

	FrameIO::async_read_frame(
		connection->socket,
		[this, remote = std::move(remote), connection = std::move(connection)](asio::error_code error, std::optional<Frame> msg) {
			if (error || !msg) {
				connection->socket.close();

				unregister_connection(connection);
				return;
			}

			message_queue_.push(Message{
				.sender = remote,
				.header = msg->header,
				.buffer = std::move(msg->buffer),
			});

			if (status_ == NetworkStatus::ACTIVE) {
				start_reading(connection);
			}
		});
}

void Network::unregister_connection(std::shared_ptr<Connection> connection) {
	if (!connection->remote) {
		pending_connections_.erase(connection);
		return;
	}

	const NodeIdentity &remote = *connection->remote;

	auto erase_if_same = [&connection, &remote](auto &registry) {
		auto it = registry.find(remote.id);

		if (it != registry.end() &&
			it->second == connection) {
			registry.erase(it);
		}
	};

	switch (remote.role) {
	case NodeRole::CLIENT:
		erase_if_same(client_connections_);
		break;

	case NodeRole::METADATA:
		erase_if_same(metadata_connections_);
		break;

	case NodeRole::STORAGE:
		erase_if_same(storage_connections_);
		break;
	}

	pending_connections_.erase(connection);
}

// void Network::run() {
// 	if (status_ == NetworkStatus::ACTIVE) return;
// 	// Read messages and put into message queue

// 	using asio::ip::tcp;

// 	tcp::endpoint local{ asio::ip::make_address(self_.host), self_.port };

// 	acceptor_.open(local.protocol());
// 	acceptor_.set_option(tcp::acceptor::reuse_address(true));
// 	acceptor_.bind(local);
// 	acceptor_.listen();

// 	status_ = NetworkStatus::ACTIVE;
// 	accept_peers();

// 	for (auto &[id, connection] : connections_) {
// 		start_reading(id, connection);
// 	}
// 	network_thread_ = std::jthread{
// 		[this]() {
// 			context_.run();
// 		}
// 	};
// }

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

// void Network::find_peers(const std::string &config_file) {
// 	assert(status_ == NetworkStatus::HANDSHAKING);
// 	self_ = Node::get_node_info(config_file);
// 	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);

// 	peers_.clear();

// 	for (const auto &peer : seed_nodes) {
// 		const NodeId id = peer.node_id;
// 		if (id.empty() || id == self_.node_id) continue;

// 		peers_.insert_or_assign(id, peer);
// 		connect_to_peer(peer);
// 	}
// }

void Network::schedule_reconnect(const Endpoint &peer) {}

void Network::register_connection(std::shared_ptr<Connection> connection, Hello hello) {
	const auto identity = hello.identity;
	if ((identity.id.empty()) ||
		(connection->expected_remote && *connection->expected_remote != identity)) {

		close_pending(connection);
		return;
	}

	connection->remote = identity;
	pending_connections_.erase(connection);

	switch (identity.role) {
	case NodeRole::CLIENT:
		client_connections_.insert_or_assign(identity.id, connection);
		break;
	case NodeRole::METADATA:
		metadata_connections_.insert_or_assign(identity.id, connection);
		break;
	case NodeRole::STORAGE:
		storage_connections_.insert_or_assign(identity.id, connection);
		break;
	}

	if (hello.advertised_endpoint) {
		known_nodes_.insert_or_assign(identity.id, std::move(*hello.advertised_endpoint));
	}

	std::cout << self_.node_id << "\t<-->\t" << identity.id << std::endl;
	start_reading(connection);
}

void Network::connect_to_peer(const Endpoint &peer) {
	using asio::ip::tcp;

	auto resolver = std::make_shared<tcp::resolver>(context_);
	auto connection = std::make_shared<Connection>(context_, true);

	connection->expected_remote = { peer.node_id, peer.role };

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

					connection->remote->id = peer.node_id;

					send_hello(connection);
					read_hello(connection);
				});
		});
}

// void Network::async_send_frame(std::shared_ptr<Connection> connection, Frame frame) {

// 	FrameIO::async_write_frame(connection->socket, std::move(frame), [this, connection](const asio::error_code &error, std::size_t) {
// 		if (!error) return;

// 		if (connection->peer_id) {
// 			auto it = connections_.find(*connection->peer_id);

// 			if (it != connections_.end() && it->second == connection) {
// 				connections_.erase(it);
// 			}
// 		}
// 		close_pending(connection);
// 	});
// }

void Network::shutdown() {
}

std::optional<Message> Network::receive_with_timeout() {
	return message_queue_.pop(std::chrono::milliseconds{ 500 });
}

// Sends this process's role, node_id, and optional
// listening endpoint so the remote side can classify the connection.
void Network::send_hello(std::shared_ptr<Connection> connection) {
	PacketWriter writer;

	writer
		.write(self_.role)
		.write_string(self_.node_id);

	const std::uint8_t has_endpoint = self_.role == NodeRole::CLIENT ? 0 : 1;

	writer.write(has_endpoint);

	if (has_endpoint == 1) {
		writer
			.write_string(self_.host)
			.write(self_.port);
	}

	Frame frame{
		MessageHeader{
			.magic = HEADER_MAGIC,
			.length = writer.length(),
			.type = MessageType::HELLO },
		writer.move_data()
	};

	async_send_frame(connection, std::move(frame));
}

void Network::read_hello(std::shared_ptr<Connection> connection) {
	FrameIO::async_read_frame(
		connection->socket,
		[this, connection](asio::error_code error, std::optional<Frame> frame) {
			if (error || !frame || frame->header.type != MessageType::HELLO) {
				close_pending(connection);
				return;
			}

			try {
				PacketReader reader{ frame->buffer };

				const auto role = reader.read<NodeRole>();
				const auto id = reader.read_string();

				const auto has_endpoint = reader.read<std::uint8_t>();

				std::optional<Endpoint> endpoint;

				if (has_endpoint == 1) {
					endpoint = Endpoint{
						.node_id = id,
						.role = role,
						.host = reader.read_string(),
						.port =
							reader.read<std::uint16_t>(),
					};
				}

				if (!reader.at_end()) {
					close_pending(connection);
					return;
				}

				Hello hello{
					.identity = NodeIdentity{
						.id = id,
						.role = role,
					},
					.advertised_endpoint = std::move(endpoint),
				};

				register_connection(connection, std::move(hello));
			} catch (...) {
				close_pending(connection);
			}
		});
}
void Network::close_pending(const std::shared_ptr<Connection> &connection) {
	pending_connections_.erase(connection);

	if (!connection->socket.is_open()) return;

	asio::error_code shutdown_error;
	connection->socket.shutdown(asio::ip::tcp::socket::shutdown_both, shutdown_error);

	if (shutdown_error && shutdown_error != asio::error::not_connected) {
		// Log shutdown_result.message() if desired.
	}

	asio::error_code close_error;
	connection->socket.close(close_error);

	if (close_error) {
		// Log close_error.message().
	}
}