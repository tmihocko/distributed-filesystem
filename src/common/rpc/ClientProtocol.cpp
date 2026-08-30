#include "ClientProtocol.hpp"
#include "Serializer.hpp"
#include "network/Node.hpp"
#include "rpc/Rpc.hpp"
#include <print>
#include <stdexcept>
#include <utility>
#include <vector>

template <ClientJob job, RpcKind kind>
static void validate_rpc_message(const ClientRpcMessage &message) {
	if (message.rpc_header.kind != kind) throw std::runtime_error("Expected other RpcKind");
	if (message.rpc_header.job != job) throw std::runtime_error("Expected other job type");
}

void FileStat::print() {
	std::println("{}", path);
	std::println("\tType: {}", is_directory ? "directory" : "file");
	std::println("\tSize: {} bytes", size);

	if (!is_directory) {
		std::println("\tObject ID: {}", object_id);
		std::println("\tStorage node 1: {}", storage_nodes[0]);
		std::println("\tStorage node 2: {}", storage_nodes[1]);
	}

	std::println("\tCreated: {:%F %T}", std::chrono::floor<std::chrono::seconds>(created_at));
	std::println("\tModified: {:%F %T}", std::chrono::floor<std::chrono::seconds>(modified_at));
}

Frame ClientProtocol::encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request) {
	BinaryWriter writer;

	writer.write(request.path);

	return Rpc::make_frame(request_id, ClientJob::CREATE_FILE, RpcKind::Request, writer.move_data());
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

	return Rpc::make_frame(request_id, ClientJob::CREATE_FILE, RpcKind::Response, writer.move_data());
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

Frame ClientProtocol::encode_remove_request(
	std::uint64_t request_id,
	const RemoveRequest &request) {
	BinaryWriter writer;
	writer.write(request.path);

	return Rpc::make_frame(request_id, ClientJob::REMOVE, RpcKind::Request, writer.move_data());
}

RemoveRequest ClientProtocol::decode_remove_request(
	const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::REMOVE, RpcKind::Request>(message);

	BinaryReader reader{ message.body };
	auto path = reader.read_string();

	reader.assert_at_end();

	return RemoveRequest{
		.path = std::move(path),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_remove_response(
	std::uint64_t request_id,
	const RemoveResponse &response) {
	BinaryWriter writer;
	writer.write(response.status);

	return Rpc::make_frame(request_id, ClientJob::REMOVE, RpcKind::Response, writer.move_data());
}

RemoveResponse ClientProtocol::decode_remove_response(
	const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::REMOVE, RpcKind::Response>(message);

	BinaryReader reader{ message.body };
	const auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return RemoveResponse{
		.status = status,
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

	return Rpc::make_frame(request_id, ClientJob::MKDIR, RpcKind::Request, writer.move_data());
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

	return Rpc::make_frame(request_id, ClientJob::MKDIR, RpcKind::Response, writer.move_data());
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

//

Frame ClientProtocol::encode_read_file_request(std::uint64_t request_id, const ReadFileRequest &request) {
	BinaryWriter writer;

	writer.write(request.path);

	return Rpc::make_frame(request_id, ClientJob::READ_FILE, RpcKind::Request, writer.move_data());
}

ReadFileRequest ClientProtocol::decode_read_file_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::READ_FILE, RpcKind::Request>(message);

	BinaryReader reader{ message.body };

	auto path = reader.read<std::string>();

	reader.assert_at_end();

	return ReadFileRequest{
		.path = std::move(path),
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_read_file_response(std::uint64_t request_id, const ReadFileResponse &response) {
	BinaryWriter writer;

	writer.write(response.status, response.file_size, response.chunk_count);

	return Rpc::make_frame(request_id, ClientJob::READ_FILE, RpcKind::Response, writer.move_data());
}

ReadFileResponse ClientProtocol::decode_read_file_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::READ_FILE, RpcKind::Response>(message);

	BinaryReader reader{ message.body };

	const auto [status, file_size, chunk_count] = reader.read<ClientStatus, std::uint64_t, std::uint32_t>();

	reader.assert_at_end();

	return ReadFileResponse{
		.status = status,
		.file_size = file_size,
		.chunk_count = chunk_count,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_read_chunk_request(std::uint64_t request_id, const ReadChunkRequest &request) {
	BinaryWriter writer;

	writer.write(request.chunk_index);

	return Rpc::make_frame(request_id, ClientJob::READ_CHUNK, RpcKind::Request, writer.move_data());
}

ReadChunkRequest ClientProtocol::decode_read_chunk_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::READ_CHUNK, RpcKind::Request>(message);

	BinaryReader reader{ message.body };

	const auto chunk_index = reader.read<std::uint32_t>();

	reader.assert_at_end();

	return ReadChunkRequest{
		.chunk_index = chunk_index,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_read_chunk_response(std::uint64_t request_id, const ReadChunkResponse &response) {
	BinaryWriter writer;

	writer.write(
		response.chunk_index,
		response.status);

	writer.write_bytes(response.data);

	return Rpc::make_frame(
		request_id,
		ClientJob::READ_CHUNK,
		RpcKind::Response,
		writer.move_data());
}

ReadChunkResponse ClientProtocol::decode_read_chunk_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::READ_CHUNK, RpcKind::Response>(message);
	BinaryReader reader{ message.body };

	const auto [chunk_index, status] = reader.read<std::uint32_t, ClientStatus>();

	auto data = reader.read_remaining();

	return ReadChunkResponse{
		.chunk_index = chunk_index,
		.status = status,
		.data = std::move(data),
		.context = RequestContext::from(message),
	};
}

//

Frame ClientProtocol::encode_write_file_request(std::uint64_t request_id, const WriteFileRequest &request) {
	BinaryWriter writer;

	writer.write(request.file_size, request.chunk_count, request.path);

	return Rpc::make_frame(request_id, ClientJob::WRITE_FILE, RpcKind::Request, writer.move_data());
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

	return Rpc::make_frame(request_id, ClientJob::WRITE_CHUNK, RpcKind::Request, writer.move_data());
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

	return Rpc::make_frame(request_id, ClientJob::WRITE_CHUNK, RpcKind::Response, writer.move_data());
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

	return Rpc::make_frame(request_id, ClientJob::WRITE_FILE, RpcKind::Response, writer.move_data());
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

//

Frame ClientProtocol::encode_rename_request(std::uint64_t request_id, const RenameRequest &request) {
	BinaryWriter writer;
	writer.write(request.old_path, request.new_path);

	return Rpc::make_frame(request_id, ClientJob::RENAME, RpcKind::Request, writer.move_data());
}
RenameRequest ClientProtocol::decode_rename_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::RENAME, RpcKind::Request>(message);

	BinaryReader reader{ message.body };
	const auto [old_path, new_path] = reader.read<std::string, std::string>();

	reader.assert_at_end();

	return RenameRequest{
		.old_path = old_path,
		.new_path = new_path,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_rename_response(std::uint64_t request_id, const RenameResponse &response) {
	BinaryWriter writer;
	writer.write(response.status);

	return Rpc::make_frame(request_id, ClientJob::RENAME, RpcKind::Response, writer.move_data());
}
RenameResponse ClientProtocol::decode_rename_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::RENAME, RpcKind::Response>(message);

	BinaryReader reader{ message.body };
	const auto status = reader.read<ClientStatus>();

	reader.assert_at_end();

	return RenameResponse{
		.status = status,
		.context = RequestContext::from(message),
	};
}

//

Frame ClientProtocol::encode_stat_request(std::uint64_t request_id, const StatRequest &request) {
	BinaryWriter writer;
	writer.write(request.path);

	return Rpc::make_frame(request_id, ClientJob::STAT, RpcKind::Request, writer.move_data());
}

StatRequest ClientProtocol::decode_stat_request(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::STAT, RpcKind::Request>(message);

	BinaryReader reader{ message.body };
	const auto path = reader.read<std::string>();

	reader.assert_at_end();

	return StatRequest{
		.path = path,
		.context = RequestContext::from(message),
	};
}

Frame ClientProtocol::encode_stat_response(std::uint64_t request_id, const StatResponse &response) {
	BinaryWriter writer;

	const auto stat = response.stat;

	writer.write(
		response.status,
		stat.path,
		stat.is_directory,
		stat.storage_nodes[0],
		stat.storage_nodes[1],
		stat.object_id,
		stat.size,
		stat.created_at,
		stat.modified_at);

	return Rpc::make_frame(request_id, ClientJob::STAT, RpcKind::Response, writer.move_data());
}
StatResponse ClientProtocol::decode_stat_response(const ClientRpcMessage &message) {
	validate_rpc_message<ClientJob::STAT, RpcKind::Response>(message);

	BinaryReader reader{ message.body };
	const auto [status, path, is_dir, node_0, node_1, obj_id, size, created, modified] =
		reader.read<
			ClientStatus, std::string, bool, NodeId, NodeId, std::string, std::uint64_t,
			std::chrono::system_clock::time_point, std::chrono::system_clock::time_point>();

	reader.assert_at_end();

	return StatResponse{
		.status = status,
		.stat = FileStat{
			.path = path,
			.is_directory = is_dir,
			.storage_nodes = { node_0, node_1 },
			.object_id = obj_id,
			.size = size,
			.created_at = created,
			.modified_at = modified,
		},
		.context = RequestContext::from(message),
	};
}
