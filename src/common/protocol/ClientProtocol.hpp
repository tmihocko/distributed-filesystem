#ifndef CLIENT_PROTOCOL_HPP
#define CLIENT_PROTOCOL_HPP

#include "network/Message.hpp"
#include "network/Node.hpp"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

namespace ClientProtocol {

using ClientSessionId = std::uint64_t;
using RequestId = std::uint64_t;

enum class Operation : std::uint8_t {
	CREATE_FILE,
	READ_FILE,
	WRITE_FILE,
	REMOVE,
	LIST,
	MKDIR,
	RENAME,
	STAT,
};

enum class RpcStatus : std::uint8_t {
	OK,
	NOT_LEADER,
	NOT_FOUND,
	ALREADY_EXISTS,
	INVALID_REQUEST,
	UNAVAILABLE,
	INTERNAL_ERROR,
};

struct RpcRequest {
	ClientSessionId client_session_id;
	RequestId request_id;
	Operation operation;
	std::vector<std::byte> payload;
};

struct RpcResponse {
	RequestId request_id;
	RpcStatus status;
	std::vector<std::byte> payload;
	std::optional<Endpoint> leader_hint;
	std::uint64_t raft_term = 0;
};

enum class ProtocolErrorCode {
	WRONG_MESSAGE_TYPE,
	MESSAGE_TOO_LARGE,
	MALFORMED_MESSAGE,
};

struct ProtocolError {
	ProtocolErrorCode code;
	std::string what;
};

std::expected<Frame, ProtocolError> encode_request(const RpcRequest &request);

std::expected<RpcRequest, ProtocolError> decode_request(const Frame &frame);

std::expected<Frame, ProtocolError> encode_response(const RpcResponse &response);

std::expected<RpcResponse, ProtocolError> decode_response(const Frame &frame);

} // namespace ClientProtocol

#endif