#include "NodeConfig.hpp"
#include "Network.hpp"
#include <asio.hpp>
#include <chrono>

std::chrono::milliseconds TIMEOUT;

int main(int argc, char *argv[]) {
	assert(argc == 2);
	std::string config_file = argv[1];

	Network &network = Network::get();
	network.find_peers(config_file);

	auto last_heartbeat = std::chrono::steady_clock::now();

	network.set_heartbeat_timer(&last_heartbeat, TIMEOUT);
	while (network.listening()) {
		auto msg = network.receive_with_timeout();

		switch (msg.header.type) {
		case MessageType::HEARTBEAT: {
			last_heartbeat = std::chrono::steady_clock::now();
			Message message; // = acknowledge
			network.send_leader(message);
			break;
		}
		case MessageType::TIMEOUT:
			// elect leader
			break;
		case MessageType::EMPTY:
			break;
		case MessageType::SHUTDOWN:
			network.shutdown();
			break;
		}
	}
}