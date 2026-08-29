#include "Client.hpp"
#include <chrono>
#include <cstdio>
#include <print>
#include <thread>

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

/**
Final todos,

READ_FILE,
Add filename/path to metadata

bytes endianness on serializer classes
*/

int main() {
	Client client{ "cl1.yaml" };

	auto res1 = client.mkdir("folder132");
	if (!res1) {
		std::println("1 Error code: {}", static_cast<int>(res1.error()));
	}

	auto res2 = client.create_file("folder1/coracao.txt");
	if (!res2) {
		std::println("2 Error code: {}", static_cast<int>(res2.error()));
	}

	auto res3 = client.write_file("secret.txt", "folder1/teiuet.txt");

	if (!res3) {
		std::println("3 Error code: {}", static_cast<int>(res3.error()));
	}

	std::println("waiting");
	std::this_thread::sleep_for(std::chrono::seconds(3));
	std::println("waiting a little bit");

	std::println("getting file stat");

	auto stat1 = client.stat("folder1/coracao.txt");

	std::println("folder1/coracao.txt");
	if (!stat1) {
		std::println("Stat failed for 'folder1/coracao.txt'. Error code: {}", static_cast<int>(stat1.error()));
	}
	stat1->print();

	auto stat2 = client.stat("folder132");

	if (!stat2) {
		std::println("Stat failed for 'folder132'. Error code: {}", static_cast<int>(stat2.error()));
	}
	std::println("folder132");
	stat2->print();

	std::println();
	std::fflush(stdout);
}