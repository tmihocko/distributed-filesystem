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
		case MessageType::CLIENT_REQUEST: {
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
Recommended next order
- Add the temporary reconnect definition.
- Remove or store the decoded endpoint.
- Test two nodes connecting and exchanging frames.
- Add duplicate handling.
- Add shutdown.
- Migrate ClientTransport onto the new Network.

After the first four items, you should finally get meaningful compiler feedback from the new implementation.
Right now the project build succeeding does not prove that this new network compiles.

what is the decoded endpoint, i lowkey forgot why i even have those, im just using connectoins can i just ignore it,




Make public API to see nodes of type NodeRole

*/