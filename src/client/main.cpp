#include "Client.hpp"
#include <cstdio>
#include <stdexcept>

// enum class ClientError : std::uint8_t {
// 	AlreadyExists, 		0
// 	ServerError,		1
// 	BadResponse,		2
// 	NotImplemented,		3
// };

int main() {
	Client client{ "cl1.yaml" };

	auto res1 = client.mkdir("folder1");
	auto res4 = client.mkdir("folder2");

	if (!res1) {
		std::println("1 Error code: {}", static_cast<int>(res1.error()));
	}
	if (!res4) {
		std::println("4 Error code: {}", static_cast<int>(res4.error()));
	}

	auto res3 = client.list();

	if (!res3) throw std::runtime_error("failed list");

	for (const auto &v : res3.value()) {
		std::print("\t\t{}", v.path);
	}
	std::println();
	std::fflush(stdout);
}