#include "ClientProtocol.hpp"
#include "network/Message.hpp"
#include "network/Packet.hpp"
#include <expected>

using namespace ClientProtocol;

std::expected<Frame, ProtocolError> ClientProtocol::encode_request(const RpcRequest &request) {
	if (request.payload.size() > std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::MESSAGE_TOO_LARGE,
			"RPC request payload is too large.",
		});
	}

	PacketWriter writer;

	writer
		.write(request.client_session_id)
		.write(request.request_id)
		.write(request.operation)
		.write(static_cast<std::uint32_t>(request.payload.size()))
		.write_bytes(request.payload);

	return Frame{
		.header = MessageHeader{
			.magic = HEADER_MAGIC,
			.length =
				static_cast<std::uint32_t>(
					writer.data().size()),
			.type = MessageType::CLIENT_REQUEST,
		},
		.buffer = writer.move_data(),
	};
}

std::expected<RpcRequest, ProtocolError> ClientProtocol::decode_request(const Frame &frame) {
	if (frame.header.type != MessageType::CLIENT_REQUEST) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::WRONG_MESSAGE_TYPE,
			"Frame is not a client request.",
		});
	}

	try {
		PacketReader reader{ frame.buffer };

		const auto [session_id, request_id, operation, payload_size] =
			reader.read<ClientSessionId, RequestId, Operation, std::uint32_t>();

		std::vector<std::byte> payload = reader.read_bytes(payload_size);

		if (!reader.at_end()) {
			return std::unexpected(ProtocolError{
				ProtocolErrorCode::MALFORMED_MESSAGE,
				"RPC message contains unexpected trailing bytes.",
			});
		}

		return RpcRequest{
			.client_session_id = session_id,
			.request_id = request_id,
			.operation = operation,
			.payload = std::move(payload),
		};
	} catch (const std::exception &error) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::MALFORMED_MESSAGE,
			error.what(),
		});
	}
}

std::expected<Frame, ProtocolError> ClientProtocol::encode_response(const RpcResponse &response) {
	if (response.payload.size() >
		std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::MESSAGE_TOO_LARGE,
			"RPC response payload is too large.",
		});
	}

	PacketWriter writer;

	writer
		.write(response.request_id)
		.write(response.status)
		.write(response.raft_term);

	const std::uint8_t has_leader_hint =
		response.leader_hint.has_value()
			? 1
			: 0;

	writer.write(has_leader_hint);

	if (response.leader_hint) {
		writer
			.write_string(response.leader_hint->node_id)
			.write_string(response.leader_hint->host)
			.write(response.leader_hint->port);
	}

	writer
		.write(static_cast<std::uint32_t>(response.payload.size()))
		.write_bytes(response.payload);

	return Frame{
		.header = MessageHeader{
			.magic = HEADER_MAGIC,
			.length =
				static_cast<std::uint32_t>(
					writer.data().size()),
			.type = MessageType::CLIENT_RESPONSE,
		},
		.buffer = writer.move_data(),
	};
}

std::expected<RpcResponse, ProtocolError> ClientProtocol::decode_response(const Frame &frame) {
	if (frame.header.type !=
		MessageType::CLIENT_RESPONSE) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::WRONG_MESSAGE_TYPE,
			"Frame is not a client response.",
		});
	}

	try {
		PacketReader reader(frame.buffer);

		const auto [request_id, status, raft_term, has_leader_hint] =
			reader.read<RequestId, RpcStatus, std::uint64_t, std::uint8_t>();

		if (has_leader_hint > 1) {
			return std::unexpected(ProtocolError{
				ProtocolErrorCode::MALFORMED_MESSAGE,
				"Invalid leader-hint flag.",
			});
		}

		std::optional<Endpoint> leader_hint;

		if (has_leader_hint == 1) {
			leader_hint = Endpoint{
				.node_id = reader.read_string(),
				.host = reader.read_string(),
				.port = reader.read<std::uint16_t>(),
			};
		}

		const std::uint32_t payload_size = reader.read<std::uint32_t>();

		std::vector<std::byte> payload = reader.read_bytes(payload_size);

		if (!reader.at_end()) {
			return std::unexpected(ProtocolError{
				ProtocolErrorCode::MALFORMED_MESSAGE,
				"RPC message contains unexpected trailing bytes.",
			});
		}

		return RpcResponse{
			.request_id = request_id,
			.status = status,
			.payload = std::move(payload),
			.leader_hint = std::move(leader_hint),
			.raft_term = raft_term,
		};

	} catch (const std::exception &error) {
		return std::unexpected(ProtocolError{
			ProtocolErrorCode::MALFORMED_MESSAGE,
			error.what(),
		});
	}
}