#ifndef OBJECTID_HPP
#define OBJECTID_HPP

#include <chrono>
#include <mutex>
#include <random>
#include <string>

namespace ObjectId {

// Ai generated UUID generator
std::string inline generate() {
	static std::mutex mutex;
	static std::mt19937_64 generator = [] {
		std::random_device random;

		const auto now = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

		std::seed_seq seed{
			random(),
			random(),
			random(),
			random(),
			static_cast<std::uint32_t>(now),
			static_cast<std::uint32_t>(now >> 32),
		};

		return std::mt19937_64(seed);
	}();

	std::array<std::uint8_t, 16> bytes;

	{
		std::scoped_lock lock(mutex);

		const std::uint64_t first = generator();
		const std::uint64_t second = generator();

		for (std::size_t i = 0; i < 8; ++i) {
			bytes[i] = static_cast<std::uint8_t>(first >> ((7 - i) * 8));

			bytes[i + 8] = static_cast<std::uint8_t>(second >> ((7 - i) * 8));
		}
	}
	// UUID version 4 and standard variant bits.
	bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);

	bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);

	static constexpr char hex[] = "0123456789abcdef";

	std::string result;
	result.reserve(36);

	for (std::size_t i = 0; i < bytes.size(); ++i) {
		if (i == 4 ||
			i == 6 ||
			i == 8 ||
			i == 10) {
			result.push_back('-');
		}

		result.push_back(hex[bytes[i] >> 4]);
		result.push_back(hex[bytes[i] & 0x0f]);
	}

	return result;
}

} // namespace ObjectId

#endif // OBJECTID_HPP