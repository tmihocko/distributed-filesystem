#include "Client.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include "Yaml.hpp"
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>

std::chrono::seconds TIMEOUT(3);

Client::Client(Endpoint self, std::span<const Endpoint> seed_nodes) : self_(self), network_(self) {
	network_.start(seed_nodes);
	for (const auto &node : seed_nodes) {
		if (node.role == NodeRole::METADATA) {
			metadata_node_id_ = node.node_id;
			return;
		}
	}
}

Client::Client(const std::string &config_file) : Client(Yaml::get_node_info(config_file), Yaml::get_seed_nodes(config_file)) {}

ClientOperation<void> Client::create_file(std::string path) {
	auto request_id = next_id();

	CreateFileRequest request{
		.path = std::move(path),
		.context = request_context(request_id),
	};

	auto frame = ClientProtocol::encode_create_file_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, &Rpc::message_is<ClientJob, RpcKind::Response>);

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(message.value());

	if (rpc_message.rpc_header.request_id != request_id) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.job != ClientJob::CREATE_FILE) return std::unexpected(ClientError::BadResponse);

	CreateFileResponse response = ClientProtocol::decode_create_file_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<std::vector<std::byte>> Client::read_file(std::string path, std::size_t byte_count) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::write_file(std::string local_path, std::string path) {
	auto request_id = next_id();
	const auto context = request_context(request_id);

	std::ifstream file(local_path, std::ios::binary);

	if (!file) return std::unexpected(ClientError::BadInput);

	const std::uint64_t file_size = std::filesystem::file_size(local_path);
	const std::uint64_t chunks = file_size / CHUNK_SIZE + (file_size % CHUNK_SIZE != 0);

	if (chunks > std::numeric_limits<std::uint32_t>::max()) return std::unexpected(ClientError::StorageFull);
	// maybe error category? if it needs this much chunks it probably wont fit anyway

	const auto chunk_count = static_cast<std::uint32_t>(chunks);

	WriteFileRequest write_req{
		.file_size = file_size,
		.chunk_count = chunk_count,
		.path = path,
		.context = context,
	};

	const auto write_req_frame = ClientProtocol::encode_write_file_request(request_id, write_req);

	network_.send(metadata_node_id_, write_req_frame);

	const auto write_frame = network_.receive_if(TIMEOUT, &Rpc::message_is<ClientJob, RpcKind::Response>);
	if (!write_frame) return std::unexpected(ClientError::Timeout);
	const auto rpc_write_response = Rpc::read_message<ClientJob>(write_frame.value());

	if (!validate_rpc_header<ClientJob::WRITE_FILE, RpcKind::Response>(rpc_write_response.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	const auto write_response = ClientProtocol::decode_write_file_response(rpc_write_response);

	if (!write_response.storage_available) return std::unexpected(ClientError::StorageFull);

	for (std::uint32_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
		std::vector<std::byte> data(CHUNK_SIZE);

		file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(CHUNK_SIZE));

		const auto bytes_read = static_cast<std::size_t>(file.gcount());

		if (bytes_read == 0) return std::unexpected(ClientError::ReadError);
		data.resize(bytes_read);

		WriteChunkRequest write_chunk{
			.chunk_index = chunk_index,
			.data = std::move(data),
			.context = context,
		};

		auto frame = ClientProtocol::encode_write_chunk_request(request_id, write_chunk);

		network_.send(metadata_node_id_, frame);

		const auto chunk_frame = network_.receive_if(TIMEOUT, &Rpc::message_is<ClientJob, RpcKind::Response>);
		if (!chunk_frame) return std::unexpected(ClientError::Timeout);
		auto rpc_chunk_response = Rpc::read_message<ClientJob>(chunk_frame.value());

		if (!validate_rpc_header<ClientJob::WRITE_CHUNK, RpcKind::Response>(rpc_write_response.rpc_header, request_id)) {
			return std::unexpected(ClientError::BadResponse);
		}

		const auto chunk_response = ClientProtocol::decode_write_chunk_response(rpc_chunk_response);
		if (chunk_response.chunk_index != chunk_index) return std::unexpected(ClientError::BadResponse);

		// handle unexpecteds, this function will end with a WriteChunkResponse
		if (chunk_response.status != ClientStatus::Success) {
			return status_to_error<void>(chunk_response.status);
		}
	}

	return {};
}

ClientOperation<void> Client::remove(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<std::vector<FileInfo>> Client::list(std::string directory) {
	auto request_id = next_id();

	ListRequest request{
		.path = directory,
		.context = request_context(request_id),
	};

	auto frame = ClientProtocol::encode_list_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, &Rpc::message_is<ClientJob, RpcKind::Response>);

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(message.value());

	if (!validate_rpc_header<ClientJob::LIST, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_list_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return response.info_vec;
	} else {
		return status_to_error<std::vector<FileInfo>>(response.status);
	}
}

ClientOperation<void> Client::mkdir(std::string path) {
	auto request_id = next_id();

	MakeDirRequest request{
		.path = path,
		.context = request_context(request_id),
	};

	Frame frame = ClientProtocol::encode_mkdir_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, &Rpc::message_is<ClientJob, RpcKind::Response>);

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(message.value());

	if (!validate_rpc_header<ClientJob::MKDIR, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_mkdir_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<void> Client::rename(std::string old_path, std::string new_path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<FileStats> Client::stat(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

std::uint64_t Client::next_id() {
	return ++current_id_;
}

RequestContext Client::request_context(std::uint64_t request_id) {
	return RequestContext{
		.request_id = request_id,
		.sender = NodeIdentity{ self_.node_id, self_.role }
	};
}
