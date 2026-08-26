#include "ClientProtocol.hpp"
#include "Serializer.hpp"
#include "rpc/Rpc.hpp"
#include <stdexcept>

template <ClientJob job, RpcKind kind>
static void validate_request(const ClientRpcMessage &message) {
	if (message.rpc_header.kind != kind) throw std::runtime_error("Expected other RpcKind");
	if (message.rpc_header.job != job) throw std::runtime_error("Expected other job type");
}

Frame ClientProtocol::encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request) {
	BinaryWriter writer;

	writer.write(request.path);

	return Rpc::make_frame(
		request_id,
		ClientJob::CREATE_FILE,
		RpcKind::Request,
		writer.move_data());
}

CreateFileRequest ClientProtocol::decode_create_file_request(const ClientRpcMessage &message) {
	validate_request<ClientJob::CREATE_FILE, RpcKind::Request>(message);
	BinaryReader reader{ message.body };
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
	BinaryWriter writer;

	writer.write(response.status);

	return Rpc::make_frame(
		request_id,
		ClientJob::CREATE_FILE,
		RpcKind::Response,
		writer.move_data());
}

CreateFileResponse ClientProtocol::decode_create_file_response(const ClientRpcMessage &message) {
	// validate_request<ClientJob::CREATE_FILE, RpcKind::Response>(message);
	// BinaryReader reader{ message.body };

	// // Implement
	// auto status = reader.read<ClientStatus>();

	// reader.assert_at_end();

	// return CreateFileResponse{
	// 	.status = status
	// };
	return {};
}

Frame encode_list_request(std::uint64_t request_id, const ListRequest &request) {
	BinaryWriter writer;

	writer.write(request.directory);

	return Rpc::make_frame(request_id, ClientJob::LIST, RpcKind::Request, writer.move_data());
}
ListRequest decode_list_request(const ClientRpcMessage &message) {
	validate_request<ClientJob::LIST, RpcKind::Request>(message);

	BinaryReader reader{ message.body };
	auto directory = reader.read_string();

	reader.assert_at_end();

	return ListRequest{
		.directory = std::move(directory),
		.context = RequestContext{
			.request_id = message.rpc_header.request_id,
			.sender = message.sender,
		}
	};
}

Frame encode_list_response(std::uint64_t request_id, const ListResponse &response) {
	return Frame{};
}
ListResponse decode_list_response(const ClientRpcMessage &message) {
	return ListResponse{};
}