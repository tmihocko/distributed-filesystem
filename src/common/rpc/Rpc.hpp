#ifndef RPC_HPP
#define RPC_HPP
#include "network/Message.hpp"
#include "Serializer.hpp"
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>

enum class RpcKind : std::uint8_t {
	Response,
	Request
};

/**
Should probably changes typename to some concept
*/
template <typename Jobs>
struct RpcHeader {
	std::uint64_t request_id;
	Jobs job;
	RpcKind kind;
};

template <typename Job>
struct RpcMessage {
	NodeIdentity sender;
	MessageHeader message_header;
	RpcHeader<Job> rpc_header;
	std::vector<std::byte> body;
};

template <typename T>
concept RpcJob =
	std::is_enum_v<T> &&
	std::same_as<std::underlying_type_t<T>, std::uint8_t>;

template <RpcJob Job>
struct RpcTraits;

namespace Rpc {

template <typename Job>
void validate_message(const Message &message) {
	if (message.header.magic != HEADER_MAGIC) throw std::runtime_error("Magic doesn't match");
	if (message.header.type != RpcTraits<Job>::message_type) throw std::runtime_error("Rpc has wrong protocol type");
	if (message.header.length != message.buffer.size()) throw std::runtime_error("RPC message length mismatch");
}

template <typename Job>
void validate_rpc_message(const RpcMessage<Job> &message) {
	if (message.rpc_header.kind != RpcKind::Request && message.rpc_header.kind != RpcKind::Response) {
		throw std::runtime_error("Invalid Rpc kind");
	}
}

template <RpcJob Job>
Frame make_frame(std::uint64_t request_id, Job job, RpcKind kind, std::span<const std::byte> buffer) {
	Frame frame;
	BinaryWriter writer;

	writer
		.write(request_id, job, kind)
		.write_bytes(buffer);

	frame.header = MessageHeader{
		.magic = HEADER_MAGIC,
		.length = writer.length(),
		.type = RpcTraits<Job>::message_type,
	};

	frame.buffer = writer.move_data();

	return frame;
}

template <typename Job>
RpcMessage<Job> read_message(Message message) {
	validate_message<Job>(message);
	BinaryReader reader{ message.buffer };

	const auto [req_id, job, kind] = reader.read<std::uint64_t, Job, RpcKind>();

	RpcMessage<Job> rpc_message = {
		.sender = std::move(message.sender),
		.message_header = message.header,
		.rpc_header = RpcHeader<Job>{ req_id, job, kind },
		.body = reader.read_remaining(),
	};

	validate_rpc_message(rpc_message);

	return rpc_message;
}

template <RpcJob Job, RpcKind Kind>
bool message_is(const Message &message) {
	if (message.header.magic != HEADER_MAGIC) return false;
	if (message.header.type != RpcTraits<Job>::message_type) return false;
	if (message.header.length != message.buffer.size()) return false;

	try {
		BinaryReader reader{ message.buffer };

		reader.read<std::uint64_t>(); // request_id
		reader.read<Job>();			  // job

		return reader.read<RpcKind>() == Kind;
	} catch (const std::exception &) {
		return false;
	}
}

} // namespace Rpc

#endif // RPC_HPP