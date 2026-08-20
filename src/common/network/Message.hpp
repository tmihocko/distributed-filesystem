#ifndef MESSAGE_HPP
#define MESSAGE_HPP
#include "Node.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr inline std::uint8_t HEADER_MAGIC = 0x69;
constexpr inline std::uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

enum class MessageType : std::uint8_t {
	// Handshake
	HELLO,

	// Client-Metadata
	CLIENT_RPC,
	// Metadata-Storage
	STORAGE_RPC,
	// Metadata-Metadata
	CONSENSUS,
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