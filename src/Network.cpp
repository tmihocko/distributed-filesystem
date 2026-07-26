#include "Network.hpp"
#include "Message.hpp"
#include "Packet.hpp"
#include "RaftState.hpp"
#include "asio/error_code.hpp"
#include "asio/ip/tcp.hpp"
#include <array>
#include <asio.hpp>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

void Network::set_heartbeat_timer(std::chrono::steady_clock::time_point *last_time, std::chrono::milliseconds timeout) {
	last_heartbeat_ = last_time;
	timeout_ = timeout;
}

void Network::start_reading(asio::ip::tcp::socket &socket) {
	async_read_frame(socket, [this, &socket](asio::error_code error, std::optional<Message> msg) {
		if (error || !msg) {
			socket.close();
			return;
		}

		message_queue_.push(std::move({ socket., *msg }));

		if (status_ == NetworkStatus::ACTIVE) {
			start_reading(socket);
		}
	});
}

void Network::run() {
	if (status_ == NetworkStatus::ACTIVE) return;
	// Read messages and put into message queue

	status_ = NetworkStatus::ACTIVE;
	for (auto &[_, socket] : connections_) {
		start_reading(socket);
	}
	network_thread_ = std::jthread{
		[this]() {
			context_.run();
		}
	};
}

void Network::accept_peers() {
	acceptor_.async_accept([this](const asio::error_code &ec, asio::ip::tcp::socket socket) {
		if (ec) {
			accept_peers();
			return;
		}
		if (status_ == NetworkStatus::ACTIVE) {
			const auto remote = socket.remote_endpoint();

			const auto key = remote.address().to_string() + ":" + std::to_string(remote.port());

			auto [it, inserted] = connections_.try_emplace(key, std::move(socket));
			if (inserted) {
				start_reading(it->second);
			}

			accept_peers();
		}
	});
}

void Network::find_peers(const std::string &config_file) {
	assert(status_ == NetworkStatus::HANDSHAKING);
	self_ = get_node_info(config_file);
	std::vector<NodeInfo> seed_nodes = get_seed_nodes(config_file);

	peers_.clear();

	std::optional<NodeInfo> found_leader;
	Term highest_term = RaftState::get().current_term();

	for (const auto &node : seed_nodes) {
		if (node.endpoint.node_id == self_.endpoint.node_id) continue;

		std::optional<PeerStatus> status = request_peer_status(node.endpoint);

		if (!status) continue;									// Peer didn't respond, probably offline
		if (status->node_id != node.endpoint.node_id) continue; // Expected different node

		if (status->term > highest_term) {
			highest_term = status->term;
			found_leader.reset();
		}

		std::optional<NodeId> claimed_leader;

		if (status->role == NodeRole::LEADER) {
			claimed_leader.emplace(status->node_id);
		} else if (status->leader_id) {
			claimed_leader.emplace(*status->leader_id);
		}

		if (!claimed_leader) continue;
		if (!found_leader) {
			found_leader.emplace(peers_.at(*claimed_leader));
		} else if (found_leader->endpoint.node_id != *claimed_leader) {
			found_leader.reset();
		}
	}
	if (highest_term > RaftState::get().current_term()) {
		RaftState::get().observe_higher_term(highest_term);
	}

	if (found_leader) {
		leader_ = *found_leader;
	} else {
		leader_.reset();
	}
}

std::optional<PeerStatus> Network::request_peer_status(const Endpoint &endpoint) {
	assert(status_ == NetworkStatus::HANDSHAKING);
	using asio::ip::tcp;

	try {
		auto [it, inserted] = connections_.try_emplace(endpoint.key(), context_);

		tcp::socket &socket = it->second;
		tcp::resolver resolver{ context_ };

		const std::string port = std::to_string(endpoint.port);
		const auto address = resolver.resolve(endpoint.host, port);

		if (inserted) {
			asio::connect(socket, address);
		}

		PeerStatusRequest request{
			self_.endpoint.node_id,
			RaftState::get().current_term()
		};

		PacketWriter payload_writer;

		payload_writer
			.write_string(request.requester_id)
			.write<Term>(request.requester_term);

		const auto &payload = payload_writer.data();

		if (payload.size() > MAX_MESSAGE_SIZE) {
			throw std::runtime_error("Message exceeds maximum size");
		}

		// Need second because we string is dynamically sized, write payload then header
		PacketWriter message_writer;

		message_writer
			.write<std::uint8_t>(HEADER_MAGIC)
			.write<std::uint32_t>(payload.size())
			.write<MessageType>(MessageType::PEER_REQUEST);

		for (const auto &byte : payload) {
			message_writer.write<std::byte>(byte);
		}

		asio::write(socket, asio::buffer(message_writer.data()));

		Message response = read_frame(socket);

		if (response.header.type != MessageType::PEER_STATUS_RESPONSE) return std::nullopt;

		PacketReader reader(response.buffer);

		const auto peer_id = reader.read_string();
		const auto peer_term = reader.read<Term>();
		const auto peer_role = reader.read<NodeRole>();
		const auto leader_id = reader.read_string();

		const PeerStatus status{ peer_id, peer_term, peer_role, leader_id };

		// Confirm that the endpoint belongs to the expected node.
		if (status.node_id != endpoint.node_id) return std::nullopt;

		return status;
	} catch (const std::exception &) {
		// Resolution, connection, I/O, or parsing failed.
		return std::nullopt;
	}
}

Message Network::read_frame(asio::ip::tcp::socket &socket) {
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

	return Message{
		header,
		std::move(payload)
	};
}

void Network::async_read_frame(
	asio::ip::tcp::socket &socket,
	std::function<void(asio::error_code, std::optional<Message>)> handler) {
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
							 handler({}, Message{ state->header, std::move(state->payload) });
							 return;
						 }

						 // Then actual message
						 asio::async_read(socket, asio::buffer(state->payload), [state, handler = std::move(handler)](const asio::error_code &error, std::size_t) mutable {
							 if (error) {
								 handler(error, std::nullopt);
								 return;
							 }

							 handler(
								 {},
								 Message{ state->header, std::move(state->payload) });
						 });
					 });
}

std::optional<Message> Network::receive_with_timeout() {
	return message_queue_.pop(timeout_);
}

std::size_t Network::send(const Endpoint &endpoint, const Message &msg) {
}

std::size_t Network::send_to_leader(const Message &msg) {
	return send(leader_.value().endpoint, msg);
}

void Network::shutdown() {
	status_ = NetworkStatus::OFF;
}

bool Network::listening() {
	return status_ == NetworkStatus::ACTIVE;
}

std::optional<NodeInfo> Network::leader() {
	return leader_;
}