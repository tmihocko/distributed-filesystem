#include "MetadataNode.hpp"
#include "network/Node.hpp"
#include <iostream>

int main(int argc, const char *argv[]) {
	if (int num_args = 2; argc != num_args) {
		std::println("Invalid number of arguments, {} required, got {}.", num_args - 1, argc - 1);
		return EXIT_FAILURE;
	}

	std::cout << "Starting: " << argv[1] << std::endl;

	std::string config_file = argv[1];

	Endpoint self = Node::get_node_info(config_file);
	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);

	MetadataNode metadata{ self, seed_nodes };

	metadata.start();
}

/**

TODO:


*/