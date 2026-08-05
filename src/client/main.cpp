#include <cstdlib>
#include "Client.hpp"
#include <cassert>
#include <print>
#include <string>
#include "network/Node.hpp"
#include "util/NumberConversion.hpp"

int main(int argc, const char *argv[]) {
	if (int num_args = 2; argc != num_args) {
		std::println("Invalid number of arguments, {} required, got {}.", num_args - 1, argc - 1);
		return EXIT_FAILURE;
	}

	std::string config_file = argv[1];

	Client &client = Client::get();

	std::vector<Endpoint> seed_nodes = Node::get_seed_nodes(config_file);

	auto connected = client.connect(seed_nodes);

	if (!connected) {
		std::println("Failed to connect.");
		return EXIT_FAILURE;
	}

	while (true) {
		// Read line

		// switch line[0] {
		// case "mkdir"
		// 	Client::mkdir(line[1])
		// 	break;
		// case "create_file":
		// 	payload = parse somehow(line[1])
		// 	Client::create_file(payload);
		// }
	}
}