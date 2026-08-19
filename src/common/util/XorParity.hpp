#ifndef XOR_PARITY_HPP
#define XOR_PARITY_HPP
#include <cstddef>
#include <span>
#include <vector>

// 2+1 Erasure coding
// parity = xor_chunks(d1, d2)
// d1 = xor_chunks(parity, d2)
// d2 = xor_chunks(parity, d1)
std::vector<std::byte> xor_parity(std::span<const std::byte> d1, std::span<const std::byte> d2);

#endif // REEDSOLOMON_HPP