#include "ClientProtocol.hpp"
#include "Serializer.hpp"
#include "rpc/Rpc.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

template <ClientJob job, RpcKind kind>
static void validate_rpc_message(const ClientRpcMessage &message) {
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
	validate_rpc_message<ClientJob::CREATE_FILE, RpcKind::Request>(message);
	BinaryReader reader{ message.body };
	auto path = reader.read_string();

	reader.assert_at_end();

	return CreateFileRequest{
		.path = std::move(path),
		.context = RequestContext::from(message),
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
	validate_rpc_message<ClientJob::CREATE_FILE, RpcKind::Response>(message);
	BinaryReader reader{ message.body };

	auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return CreateFileResponse{
		.status = status,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_list_request(std::uint64_t request_id, const ListRequest &request) {
	BinaryWriter writer;

	writer.write(request.path);

	return Rpc::make_frame(request_id, ClientJob::LIST, RpcKind::Request, writer.move_data());
}

ListRequest ClientProtocol::decode_list_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::LIST, RpcKind::Request>(message);

	BinaryReader reader{ message.body };
	auto directory = reader.read_string();

	reader.assert_at_end();

	return ListRequest{
		.path = std::move(directory),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_list_response(std::uint64_t request_id, const ListResponse &response) {
	BinaryWriter writer;

	writer.write<ClientStatus, std::uint32_t>(response.status, response.info_vec.size());

	for (const auto &info : response.info_vec) {
		writer.write(info.path, info.is_directory);
	}

	return Rpc::make_frame(request_id, ClientJob::LIST, RpcKind::Response, writer.move_data());
}

ListResponse ClientProtocol::decode_list_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::LIST, RpcKind::Response>(message);

	BinaryReader reader{ message.body };

	const auto [status, size] = reader.read<ClientStatus, std::uint32_t>();

	std::vector<FileInfo> info_vec(size);

	for (int i = 0; i < static_cast<int>(size); i++) {
		auto [path, is_dir] = reader.read<std::string, bool>();
		info_vec[i] = FileInfo{
			.path = std::move(path),
			.is_directory = is_dir,
		};
	}

	reader.assert_at_end();

	return ListResponse{
		.status = status,
		.info_vec = std::move(info_vec),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_mkdir_request(std::uint64_t request_id, const MakeDirRequest &response) {
	BinaryWriter writer;

	writer.write(response.path);

	return Rpc::make_frame(
		request_id,
		ClientJob::MKDIR,
		RpcKind::Request,
		writer.move_data());
}

MakeDirRequest ClientProtocol::decode_mkdir_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::MKDIR, RpcKind::Request>(message);
	BinaryReader reader{ message.body };
	auto path = reader.read_string();

	reader.assert_at_end();

	return MakeDirRequest{
		.path = std::move(path),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_mdkir_response(std::uint64_t request_id, const MakeDirResponse &response) {
	BinaryWriter writer;

	writer.write(response.status);

	return Rpc::make_frame(
		request_id,
		ClientJob::MKDIR,
		RpcKind::Response,
		writer.move_data());
}

MakeDirResponse ClientProtocol::decode_mkdir_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::MKDIR, RpcKind::Response>(message);
	BinaryReader reader{ message.body };

	auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return MakeDirResponse{
		.status = status,
		.context = RequestContext::from(message),
	};
}
Frame ClientProtocol::encode_write_file_request(std::uint64_t request_id, const WriteFileRequest &request) {
	BinaryWriter writer;

	writer.write(request.file_size, request.chunk_count, request.path);

	return Rpc::make_frame(
		request_id,
		ClientJob::WRITE_FILE,
		RpcKind::Request,
		writer.move_data());
}

WriteFileRequest ClientProtocol::decode_write_file_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::WRITE_FILE, RpcKind::Request>(message);

	BinaryReader reader{ message.body };

	auto [file_size, chunk_count, path] = reader.read<std::uint64_t, std::uint32_t, std::string>();

	reader.assert_at_end();

	return WriteFileRequest{
		.file_size = file_size,
		.chunk_count = chunk_count,
		.path = std::move(path),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_write_chunk_request(
	std::uint64_t request_id,
	const WriteChunkRequest &request) {
	BinaryWriter writer;

	writer.write(request.chunk_index).write_bytes(request.data);

	return Rpc::make_frame(
		request_id,
		ClientJob::WRITE_CHUNK,
		RpcKind::Request,
		writer.move_data());
}

WriteChunkRequest ClientProtocol::decode_write_chunk_request(
	const ClientRpcMessage &message) {
	validate_rpc_message<
		ClientJob::WRITE_CHUNK,
		RpcKind::Request>(message);

	BinaryReader reader{ message.body };

	const auto chunk_index = reader.read<std::uint32_t>();

	auto data = reader.read_remaining();

	return WriteChunkRequest{
		.chunk_index = chunk_index,
		.data = std::move(data),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_write_chunk_response(
	std::uint64_t request_id,
	const WriteChunkResponse &response) {
	BinaryWriter writer;

	writer.write(response.chunk_index, response.status);

	return Rpc::make_frame(
		request_id,
		ClientJob::WRITE_CHUNK,
		RpcKind::Response,
		writer.move_data());
}

WriteChunkResponse ClientProtocol::decode_write_chunk_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::WRITE_CHUNK, RpcKind::Response>(message);

	BinaryReader reader{ message.body };

	const auto [chunk_index, status] = reader.read<std::uint32_t, ClientStatus>();

	reader.assert_at_end();

	return WriteChunkResponse{
		.chunk_index = chunk_index,
		.status = status,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_write_file_response(std::uint64_t request_id, const WriteFileResponse &response) {
	BinaryWriter writer;
	writer.write(response.status);

	return Rpc::make_frame(
		request_id,
		ClientJob::WRITE_FILE,
		RpcKind::Response,
		writer.move_data());
}

WriteFileResponse ClientProtocol::decode_write_file_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::WRITE_FILE, RpcKind::Response>(message);

	BinaryReader reader{ message.body };
	const auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return WriteFileResponse{
		.status = status,
		.context = RequestContext::from(message),
	};
}