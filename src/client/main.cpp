#include <cstdlib>
#include "Client.hpp"
#include <cassert>
#include <print>
#include <string>
#include "network/Node.hpp"

int main(int argc, const char *argv[]) {
	if (int num_args = 2; argc != num_args) {
		std::println("Invalid number of arguments, {} required, got {}.", num_args - 1, argc - 1);
		return EXIT_FAILURE;
	}

	std::string config_file = argv[1];

	auto self = Node::get_node_info(config_file);
	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);

	Client client{ self, seed_nodes };

	while (true) {
		// Mainly synchronous:
		// take request,
		// wait for ack/finish,
		// take in next reqeust
	}
}