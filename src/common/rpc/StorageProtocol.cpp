#include "StorageProtocol.hpp"
#include "Serializer.hpp"
#include "rpc/ClientProtocol.hpp"

template <StorageJob Job, RpcKind Kind>
static void validate_storage_message(const StorageRpcMessage &message) {
	if (message.rpc_header.job != Job) throw std::runtime_error("Unexpected storage job");
	if (message.rpc_header.kind != Kind) throw std::runtime_error("Unexpected storage RPC kind");
}

Frame StorageProtocol::encode_put_request(std::uint64_t request_id, const PutRequest &request) {
	BinaryWriter writer;

	writer
		.write(
			request.object_id,
			request.chunk_index,
			request.final_chunk)
		.write_bytes(request.data);

	return Rpc::make_frame(
		request_id,
		StorageJob::PUT,
		RpcKind::Request,
		writer.move_data());
}

PutRequest StorageProtocol::decode_put_request(const StorageRpcMessage &message) {
	validate_storage_message<StorageJob::PUT, RpcKind::Request>(message);

	BinaryReader reader{ message.body };

	const auto [object_id, chunk_index, final_chunk] = reader.read<std::string, std::uint32_t, bool>();

	auto data = reader.read_remaining();

	return PutRequest{
		.object_id = std::move(object_id),
		.chunk_index = chunk_index,
		.final_chunk = final_chunk,
		.data = std::move(data),
		.context = RequestContext::from(message)
	};
}

Frame StorageProtocol::encode_put_response(
	std::uint64_t request_id,
	const PutResponse &response) {
	BinaryWriter writer;

	writer.write(response.chunk_index, response.size_available, response.status);

	return Rpc::make_frame(
		request_id,
		StorageJob::PUT,
		RpcKind::Response,
		writer.move_data());
}

PutResponse StorageProtocol::decode_put_response(
	const StorageRpcMessage &message) {
	validate_storage_message<StorageJob::PUT, RpcKind::Response>(message);

	BinaryReader reader{ message.body };

	const auto [chunk_index, size_available, status] = reader.read<std::uint32_t, std::uint64_t, StorageStatus>();

	reader.assert_at_end();

	return PutResponse{
		.chunk_index = chunk_index,
		.size_available = size_available,
		.status = status,
	};
}

Frame StorageProtocol::encode_delete_request(std::uint64_t request_id, const DeleteRequest &request) {
	BinaryWriter writer;
	writer.write(request.object_id);

	return Rpc::make_frame(
		request_id,
		StorageJob::DELETE,
		RpcKind::Request,
		writer.move_data());
}

DeleteRequest StorageProtocol::decode_delete_request(const StorageRpcMessage &message) {
	validate_storage_message<StorageJob::DELETE, RpcKind::Request>(message);
	BinaryReader reader{ message.body };

	const auto object_id = reader.read<std::string>();

	reader.assert_at_end();

	return DeleteRequest{
		.object_id = object_id,
		.context = RequestContext::from(message),
	};
}

Frame StorageProtocol::encode_delete_response(std::uint64_t request_id, const DeleteResponse &response) {
	BinaryWriter writer;
	writer.write(response.status);

	return Rpc::make_frame(
		request_id,
		StorageJob::DELETE,
		RpcKind::Response,
		writer.move_data());
}

DeleteResponse StorageProtocol::decode_delete_response(const StorageRpcMessage &message) {
	validate_storage_message<StorageJob::DELETE, RpcKind::Response>(message);

	BinaryReader reader{ message.body };
	const auto status = reader.read<StorageStatus>();

	reader.assert_at_end();

	return DeleteResponse{
		.status = status,
	};
}
