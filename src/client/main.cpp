#include "Client.hpp"
#include <stdexcept>

int main() {
	Client client{ "cl1.yaml" };

	auto result = client.create_file("yaml.hpp");

	if (!result) {
		throw std::runtime_error("hi");
	}
}