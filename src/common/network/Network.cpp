#include "Network.hpp"
#include "Message.hpp"
#include "Packet.hpp"
#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/tcp.hpp"
#include "network/FrameIO.hpp"
#include <asio.hpp>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>

// This is where node_id and connection are "strongly binded"
void Network::start_reading(NodeId node_id, std::shared_ptr<Connection> connection) {
	FrameIO::async_read_frame(
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
				.sender = node_id,
				.header = msg->header,
				.buffer = std::move(msg->buffer),
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

	tcp::endpoint local{ asio::ip::make_address(self_.host), self_.port };

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
	self_ = Node::get_node_info(config_file);
	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);

	peers_.clear();

	for (const auto &peer : seed_nodes) {
		const NodeId id = peer.node_id;
		if (id.empty() || id == self_.node_id) continue;

		peers_.insert_or_assign(id, peer);
		connect_to_peer(peer);
	}
}

void Network::schedule_reconnect(const Endpoint &peer) {}

void Network::register_connection(std::shared_ptr<Connection> connection, Endpoint peer) {
	const NodeId id = peer.node_id;
	if ((id.empty() || id == self_.node_id) ||
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

	std::cout << self_.node_id << "\t<-->\t" << id << std::endl;
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

void Network::async_send_frame(std::shared_ptr<Connection> connection, Frame frame) {

	FrameIO::async_write_frame(connection->socket, std::move(frame), [this, connection](const asio::error_code &error, std::size_t) {
		if (!error) return;

		if (connection->peer_id) {
			auto it = connections_.find(*connection->peer_id);

			if (it != connections_.end() && it->second == connection) {
				connections_.erase(it);
			}
		}
		close_pending(connection);
	});
}

void Network::shutdown() {
}

std::optional<Message> Network::receive_with_timeout() {
	return message_queue_.pop(std::chrono::milliseconds{ 500 });
}

void Network::send_hello(std::shared_ptr<Connection> connection) {
	PacketWriter writer;

	writer
		.write_string(self_.node_id)
		.write_string(self_.host)
		.write<std::uint16_t>(self_.port);

	Frame frame{
		MessageHeader{
			HEADER_MAGIC,
			writer.length(),
			MessageType::HELLO },
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

				Endpoint peer{
					.node_id = reader.read_string(),
					.host = reader.read_string(),
					.port = reader.read<std::uint16_t>(),
				};

				register_connection(connection, std::move(peer));
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