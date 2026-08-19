#include "network/Message.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "network/Packet.hpp"
#include <iostream>

int main(int argc, const char *argv[]) {
	if (int num_args = 2; argc != num_args) {
		std::println("Invalid number of arguments, {} required, got {}.", num_args - 1, argc - 1);
		return EXIT_FAILURE;
	}

	std::cout << "Starting: " << argv[1] << std::endl;

	std::string config_file = argv[1];

	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);
	Endpoint self = Node::get_node_info(config_file);

	Network<NodeRole::METADATA> network(self);
	network.start(seed_nodes);

	while (true) {
		auto message = network.receive();

		switch (message.header.type) {
		case MessageType::HEARTBEAT:
			std::cout << "beat" << std::endl;
			break;
		case MessageType::CLIENT_CREATE_FILE: {
			if (message.sender.role != NodeRole::CLIENT) break;

			PacketReader reader{ std::move(message.buffer) };

			// Find type of client request,
			// Metadata::Do_Stuff<req>()

			break;
		}
		default:
			break;
		}
		// RPC switch
		// Do stuff
	}
}

/**

TODO:
CLient
Client Transport, etc.

Do SOME stuff on Metadata node,
Define specifically requirements of metadata and storage,
Erasure coding 3+S

*/