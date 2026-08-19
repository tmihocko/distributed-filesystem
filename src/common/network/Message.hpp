#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include "Node.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr inline std::uint8_t HEADER_MAGIC = 0x69;
constexpr inline std::uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

enum class MessageType : std::uint8_t {
	HELLO = 0,

	HEARTBEAT = 50,
	ELECTION = 51,

	// Client -> Metadata node requests
	CLIENT_CREATE_FILE = 100,
	CLIENT_READ_FILE = 101,
	CLIENT_WRITE_FILE = 102,
	CLIENT_REMOVE = 103,
	CLIENT_LIST = 104,
	CLIENT_MKDIR = 105,
	CLIENT_RENAME = 106,
	CLIENT_STAT = 107,

	TIMEOUT = 253,
	EMPTY = 254,
	SHUTDOWN = 255,
};

struct __attribute__((packed)) MessageHeader {
	std::uint8_t magic;
	std::uint32_t length;

	MessageType type;
};

constexpr inline std::size_t MESSAGE_HEADER_SIZE =
	sizeof(std::uint8_t) +
	sizeof(std::uint32_t) +
	sizeof(MessageType);

static_assert(sizeof(MessageHeader) == MESSAGE_HEADER_SIZE);

struct Frame {
	MessageHeader header;
	std::vector<std::byte> buffer;
};

struct Message {
	NodeIdentity sender;
	MessageHeader header;
	std::vector<std::byte> buffer;
};

#endif // MESSAGE_HPP