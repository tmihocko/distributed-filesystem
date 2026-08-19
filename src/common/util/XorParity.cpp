#include "XorParity.hpp"
#include <stdexcept>

// 2+1 Erasure coding
// parity = xor_chunks(d1, d2)
// d1 = xor_chunks(parity, d2)
// d2 = xor_chunks(parity, d1)
std::vector<std::byte> xor_chunks(std::span<const std::byte> d1, std::span<const std::byte> d2) {
	if (d1.size() != d2.size()) {
		throw std::invalid_argument("Data shards must be equal size.");
	}

	std::vector<std::byte> result(d1.size());

	for (std::size_t i = 0; i < d1.size(); i++) {
		result[i] = d1[i] ^ d2[i];
	}

	return result;
}