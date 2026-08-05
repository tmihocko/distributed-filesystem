#include "shared/network/Network.hpp"
#include <asio.hpp>
#include <chrono>

int main(int argc, char *argv[]) {
	assert(argc == 2);
	std::string config_file = argv[1];

	Network &network = Network::get();
	// Raft &raft = Raft::get();

	network.find_peers(config_file);

	auto last_heartbeat = std::chrono::steady_clock::now();

	network.run();

	while (true) {
		std::optional<Message> msg = network.receive_with_timeout();

		if (!msg) {
			// leader missing, re-elect protocol
			// Raft.heartbeat_timeout();
			continue;
		}

		switch (msg->header.type) {
		case MessageType::HEARTBEAT: {
			last_heartbeat = std::chrono::steady_clock::now();
			Message message; // = acknowledge
			// Raft.send_to_leader(message);
			break;
		}
		case MessageType::EMPTY:
			break;
		case MessageType::SHUTDOWN:
			network.shutdown();
			break;
		default:
			break;
		}
	}
}