#include "Client.hpp"
#include <cstdio>

// enum class ClientError : std::uint8_t {
// 0 	AlreadyExists,
// 1 	ServerError,
// 2 	BadResponse,
// 3 	BadInput,
// 4 	NotImplemented,
// 5 	Timeout,
// 6 	StorageFull,
// 7 	ReadError,
// };

int main() {
	Client client{ "cl1.yaml" };

	auto res1 = client.mkdir("folder1");
	if (!res1) {
		std::println("1 Error code: {}", static_cast<int>(res1.error()));
	}

	auto res2 = client.create_file("folder1/teiuet.txt");
	if (!res2) {
		std::println("2 Error code: {}", static_cast<int>(res2.error()));
	}

	auto res3 = client.write_file("secret.txt", "folder1/secret_work.txt");

	if (!res3) {
		std::println("3 Error code: {}", static_cast<int>(res3.error()));
	}

	std::println();
	std::fflush(stdout);
}