#include "Client.hpp"
#include <stdexcept>

int main() {
	Client client{ "cl1.yaml" };

	auto res1 = client.create_file("josué.png");

	if (!res1) {
		std::println("Error code: {}", static_cast<int>(res1.error()));
	} else {
		std::println("Success!");
	}
	auto res2 = client.list();

	if (!res2) throw std::runtime_error("failed list");

	for (const auto &v : res2.value()) {
		std::println("{}, {}", v.is_directory, v.path);
	}
}