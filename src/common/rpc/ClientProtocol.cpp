#include "ClientProtocol.hpp"
#include "network/Packet.hpp"
#include "rpc/Rpc.hpp"
#include <optional>
#include <stdexcept>

template <ClientJob job, RpcKind kind>
static void validate_request(const ClientRpcMessage &message) {
	if (message.rpc_header.kind != kind) throw std::runtime_error("Expected other RpcKind");
	if (message.rpc_header.job != job) throw std::runtime_error("Expected other job type");
}

Frame ClientProtocol::encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request) {
	PacketWriter writer{ request.path };

	return Rpc::make_frame(
		request_id,
		ClientJob::CREATE_FILE,
		RpcKind::Request,
		writer.move_data());
}

CreateFileRequest ClientProtocol::decode_create_file_request(const ClientRpcMessage &message) {
	validate_request<ClientJob::CREATE_FILE, RpcKind::Request>(message);
	PacketReader reader{ message.body };
	auto path = reader.read_string();

	reader.assert_at_end();

	return CreateFileRequest{
		.path = std::move(path),
		.context = {
			.request_id = message.rpc_header.request_id,
			.sender = message.sender,
		}
	};
}

Frame ClientProtocol::encode_create_file_response(std::uint64_t request_id, const CreateFileResponse &response) {
	PacketWriter writer{};

	writer.write(response.status);

	return Rpc::make_frame(
		request_id,
		ClientJob::CREATE_FILE,
		RpcKind::Response,
		writer.move_data());
}

CreateFileResponse ClientProtocol::decode_create_file_response(const ClientRpcMessage &message) {
	validate_request<ClientJob::CREATE_FILE, RpcKind::Response>(message);
	PacketReader reader{ message.body };

	// Implement
	auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return CreateFileResponse{
		.status = status
	};
}

LeaderHintResponse ClientProtocol::decode_leader_hint_response(const ClientRpcMessage &message) {
	PacketReader reader{ message.body };

	bool leader_known = reader.read<std::uint8_t>() == 1;

	if (leader_known) {
		auto leader_id = reader.read_string();
		return LeaderHintResponse{ leader_id };
	} else {
		return LeaderHintResponse{ std::nullopt };
	}
}

Frame ClientProtocol::encode_leader_hint_response(std::uint64_t request_id, const LeaderHintResponse &response) {
	PacketWriter writer;

	if (response.leader_id) {
		writer.write<std::uint8_t>(1).write_string(*response.leader_id);
	} else {
		writer.write<std::uint8_t>(0);
	}

	return Rpc::make_frame(
		request_id,
		ClientJob::NOT_LEADER,
		RpcKind::Response,
		writer.move_data());
}