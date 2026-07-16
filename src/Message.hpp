#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr inline std::uint8_t HEADER_MAGIC = 0x69;

enum class MessageType : std::uint8_t {
	HEARTBEAT = 0,
	TIMEOUT = 253,
	EMPTY = 254,
	SHUTDOWN = 255,
};

struct MessageHeader {
	std::uint8_t magic;
	std::uint16_t length;
	MessageType type;
};

struct Message {
	MessageHeader header;
	std::vector<std::byte> buffer;
};

#endif // MESSAGE_HPP