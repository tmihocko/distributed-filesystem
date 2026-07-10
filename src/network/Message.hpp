#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstddef>
#include <cstdint>
#include <asio.hpp>

enum class MessageType : uint16_t {
	Heartbeat = 1,
	Acknowledge = 2,
	FindPeers = 3,
	TYPE_END = 8, // For iteration, do not send over network
};

#pragma pack(push, 1)
struct MessageHeader {
	uint32_t length;  // length of payload AFTER this header, in bytes
	MessageType type; // what kind of message this is
	uint32_t id;	  // for matching responses to requests
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(MessageHeader);

struct Message {
	MessageHeader header;
	std::vector<std::byte> buffer;

	size_t size() const {
		return HEADER_SIZE + buffer.size();
	}

	friend std::ostream &operator<<(std::ostream &os, const Message &msg) {
		os << "TYPE: " << static_cast<uint16_t>(msg.header.type) << "\nID: " << msg.header.id << "\nDATA: " << msg.buffer.data();
		return os;
	}
};

#endif // MESSAGE_HPP