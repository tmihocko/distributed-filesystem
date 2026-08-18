#ifndef NETWORK_TPP
#define NETWORK_TPP
#include "asio/post.hpp"
#include "network/Message.hpp"
#ifndef NETWORK_HPP
#include "Network.hpp"
#endif
#include "network/Node.hpp"
#include "network/Packet.hpp"
#include "network/FrameIO.hpp"

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::start(std::span<const Endpoint> seed_nodes) {
	connections_.clear();

	for (const auto &endpoint : seed_nodes) {
		if (!should_connect(endpoint)) continue;

		connect(endpoint);
		// Connect with peer
		// Add to known_nodes_ and peers_
	}

	start_accepting();

	network_thread_ = std::jthread([this]() {
		context_.run();
	});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::send(NodeId node_id, Frame frame) {
	asio::post(context_, [this, node_id = std::move(node_id), frame = std::move(frame)]() mutable {
		auto it = connections_.find(node_id);

		if (it == connections_.end()) return;

		it->second->enqueue_write(std::move(frame));
	});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::broadcast(NodeRole role, Frame frame) {
	assert(can_connect_with_role(role));
	asio::post(context_, [this, role, frame = std::move(frame)]() mutable {
		for (const auto &[id, connection] : connections_) {
			if (connection->remote->role == role) {
				connection->enqueue_write(frame);
			}
		}
	});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
Message Network<SelfRole, PeerRoles...>::receive() {
	return message_queue_.pop();
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
template <typename Rep, typename Period>
std::optional<Message> Network<SelfRole, PeerRoles...>::receive(std::chrono::duration<Rep, Period> timeout) {
	return message_queue_.pop(timeout);
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
constexpr bool Network<SelfRole, PeerRoles...>::can_connect_with_role(NodeRole role) {
	return ((role == PeerRoles) || ...);
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
bool Network<SelfRole, PeerRoles...>::should_connect(const Endpoint &endpoint) {
	return can_connect_with_role(endpoint.role) && self_.node_id < endpoint.node_id;
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::connect(const Endpoint &endpoint) {
	using asio::ip::tcp;

	auto resolver = std::make_shared<tcp::resolver>(context_);
	auto connection = std::make_shared<Connection>(
		tcp::socket(context_),
		ConnectionOrigin::Outgoing,
		std::bind_front(&Network::fail_connection, this));

	connection->expected_remote = NodeIdentity{
		endpoint.node_id,
		endpoint.role
	};

	pending_connections_.insert(connection);

	resolver->async_resolve(
		endpoint.host,
		std::to_string(endpoint.port),
		[this, endpoint, resolver, connection](asio::error_code error, tcp::resolver::results_type results) {
			if (error) {
				fail_connection(connection);
				schedule_reconnect(endpoint);
				return;
			}

			asio::async_connect(
				connection->socket,
				results,
				[this, endpoint, connection](
					asio::error_code error,
					const tcp::endpoint &) {
					if (error) {
						fail_connection(connection);
						schedule_reconnect(endpoint);
						return;
					}

					handshake(connection);
				});
		});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::start_accepting() {
	asio::ip::tcp::endpoint local{ asio::ip::make_address(self_.host), self_.port };

	acceptor_.open(local.protocol());
	acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.bind(local);
	acceptor_.listen();

	auto accept_one = [this](this auto self) {
		acceptor_.async_accept([this, self](const asio::error_code &error, asio::ip::tcp::socket socket) mutable {
			if (error) {
				if (error != asio::error::operation_aborted && acceptor_.is_open()) {
					self();
				}
				return;
			}
			// Queue the next accept immediately. Handshaking this peer
			// must not prevent other peers from connecting.
			self();

			auto connection = std::make_shared<Connection>(
				std::move(socket),
				ConnectionOrigin::Incoming,
				std::bind_front(&Network::fail_connection, this));

			pending_connections_.insert(connection);
			handshake(connection);
		});
	};

	accept_one();
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::register_connection(ConnectionPtr connection, NodeIdentity remote) {
	connection->remote = remote;

	pending_connections_.erase(connection);
	connections_.insert_or_assign(remote.id, connection);

	start_reading(connection);
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::start_reading(ConnectionPtr connection) {
	if (!connection->remote) {
		fail_connection(connection);
		return;
	}

	const NodeIdentity remote = *connection->remote;

	FrameIO::async_read_frame(
		connection->socket,
		[this, connection, remote](
			const asio::error_code &error,
			std::optional<Frame> frame) {
			if (error || !frame) {
				fail_connection(connection);
				return;
			}

			// HELLO is only valid during handshake.
			if (frame->header.type == MessageType::HELLO) {
				fail_connection(connection);
				return;
			}

			message_queue_.push(Message{
				.sender = remote,
				.header = frame->header,
				.buffer = std::move(frame->buffer),
			});

			start_reading(connection);
		});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::handshake(ConnectionPtr connection) {
	PacketWriter writer;

	const std::uint8_t has_endpoint = !self_.host.empty() && self_.port != 0;

	writer
		.write(SelfRole)
		.write_string(self_.node_id)
		.write(has_endpoint);

	if (has_endpoint) {
		writer
			.write_string(self_.host)
			.write(self_.port);
	}

	Frame hello_frame = {
		MessageHeader{
			.magic = HEADER_MAGIC,
			.length = writer.length(),
			.type = MessageType::HELLO,
		},
		writer.move_data()
	};

	FrameIO::async_write_frame(
		connection->socket,
		std::move(hello_frame),
		[this, connection](const asio::error_code &write_error, std::size_t) {
			if (write_error) {
				fail_connection(connection);
				return;
			}

			FrameIO::async_read_frame(
				connection->socket,
				[this, connection](const asio::error_code &read_error, std::optional<Frame> frame) {
					if (read_error || !frame || frame->header.type != MessageType::HELLO) {
						fail_connection(connection);
						return;
					}

					try {
						PacketReader reader{ frame->buffer };

						const NodeRole role = reader.read<NodeRole>();
						const NodeId id = reader.read_string();
						const std::uint8_t has_endpoint = reader.read<std::uint8_t>();
						const bool valid_role =
							role == NodeRole::CLIENT ||
							role == NodeRole::METADATA ||
							role == NodeRole::STORAGE;

						if (!valid_role ||
							!can_connect_with_role(role) ||
							id.empty() ||
							has_endpoint > 1) {
							fail_connection(connection);
							return;
						}

						std::optional<Endpoint> endpoint;

						if (has_endpoint == 1) {
							std::string host = reader.read_string();
							std::uint16_t port = reader.read<std::uint16_t>();

							if (host.empty() || port == 0) {
								fail_connection(connection);
								return;
							}

							endpoint = Endpoint{
								.node_id = id,
								.role = role,
								.host = std::move(host),
								.port = port,
							};
						}

						if (!reader.at_end()) {
							fail_connection(connection);
							return;
						}

						NodeIdentity remote{ id, role };

						if (connection->expected_remote && *connection->expected_remote != remote) {
							fail_connection(connection);
							return;
						}

						register_connection(connection, std::move(remote));

					} catch (const std::exception &) {
						fail_connection(connection);
					}
				});
		});
}

template <NodeRole SelfRole, NodeRole... PeerRoles>
void Network<SelfRole, PeerRoles...>::fail_connection(ConnectionPtr connection) {
	connection->close();

	pending_connections_.erase(connection);

	if (!connection->remote) return;

	auto it = connections_.find(connection->remote->id);

	if (it != connections_.end() && it->second == connection) {
		connections_.erase(it);
	}
}

#endif // NETWORK_TPP