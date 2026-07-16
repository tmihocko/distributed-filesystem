#include "Network.hpp"
#include "Message.hpp"
#include "NodeConfig.hpp"
#include "Packet.hpp"
#include "PeerStatus.hpp"
#include "RaftState.hpp"
#include <asio.hpp>
#include <cassert>

void Network::set_heartbeat_timer(std::chrono::steady_clock::time_point *last_time, std::chrono::milliseconds timeout) {
	last_heartbeat_ = last_time;
	timeout_ = timeout;
}

// void Network::find_peers(const std::string &config_file) {
// 	self_ = get_node_info(config_file);
// 	std::vector<NodeInfo> seed_nodes = get_seed_nodes(config_file);

// 	peers_.clear();

// 	std::optional<NodeInfo> found_leader;
// 	Term highest_term = RaftState::get().current_term();

// 	for (const auto &node : seed_nodes) {
// 		if (node.node_id == self_.node_id) continue;

// 		std::optional<PeerStatus> status = request_peer_status(node.endpoint);

// 		if (!status) continue;						   // Peer didn't respond, probably offline
// 		if (status->node_id != node.node_id) continue; // Expected different node

// 		if (status->term > highest_term) {
// 			highest_term = status->term;
// 			found_leader.reset();
// 		}

// 		std::optional<NodeId> claimed_leader;

// 		if (status->role == NodeRole::LEADER) {
// 			claimed_leader.emplace(status->node_id);
// 		} else {
// 			claimed_leader.emplace(*status->leader_id);
// 		}

// 		if (!claimed_leader) continue;
// 		if (!found_leader) {
// 			found_leader.emplace(*claimed_leader);
// 		} else if (*found_leader != *claimed_leader) {
// 			found_leader.reset();
// 		}
// 	}
// 	if (highest_term > RaftState::get().current_term()) {
// 		RaftState::get().observe_higher_term(highest_term);
// 	}

// 	if (found_leader) {
// 		leader_ = *found_leader;
// 	} else {
// 		leader_.reset();
// 	}
// }

std::optional<PeerStatus> Network::request_peer_status(const Endpoint &endpoint) {
	using asio::ip::tcp;

	try {
		auto [it, inserted] = sockets_.try_emplace(endpoint.key(), context_);

		tcp::socket &socket = it->second;
		tcp::resolver resolver{ context_ };

		std::string port = std::to_string(endpoint.port);

		const tcp::resolver::results_type address = resolver.resolve(endpoint.host, port);

		if (inserted) {
			asio::connect(socket, address);
		}

		Term current_term = RaftState::get().current_term();

		PeerStatusRequest request{ self_.endpoint.node_id, current_term };

		PacketWriter writer;

		writer.write(request.requester_id);
		writer.write<Term>(request.requester_term);

		asio::write(socket, asio::buffer(writer.data()));

		std::vector<std::byte> incoming;
		// Assumes every serialized message ends with '\n'.
		// Change this later to take in header
		asio::read_until(socket, asio::dynamic_buffer(incoming), '\n');

		PacketReader reader(incoming);

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

Message Network::receive_with_timeout() {
	if (message_queue_.empty()) {
		// wait until any endpoints gives a complete message
		// concurrency bullshit
	} else {
		return message_queue_.front();
	}
}

std::size_t Network::send(const Endpoint &endpoint, const Message &) {}

std::size_t Network::send_leader(const Message &msg) {
	return send(leader_.value().endpoint, msg);
}

void Network::shutdown() {
	listening_ = false;
}

bool Network::listening() {
	return listening_;
}

std::optional<NodeInfo> Network::leader() {
	return leader_;
}