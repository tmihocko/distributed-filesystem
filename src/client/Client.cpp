#include "Client.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include "Yaml.hpp"
#include <cstdint>
#include <expected>

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
		.context = {
			.request_id = request_id,
			.sender = NodeIdentity{ self_.node_id, self_.role } },
	};

	auto frame = ClientProtocol::encode_create_file_request(request_id, request);

	network_.send(metadata_node_id_, frame);

	auto message = network_.receive();
	auto rpc_message = Rpc::read_message<ClientJob>(message);

	if (rpc_message.rpc_header.request_id != request_id) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.job != ClientJob::CREATE_FILE) return std::unexpected(ClientError::BadResponse);

	CreateFileResponse response = ClientProtocol::decode_create_file_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return std::unexpected(ClientError::ServerError);
	}
}

ClientOperation<std::vector<std::byte>> Client::read_file(std::string path, std::size_t byte_count) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::write_file(std::string path, std::span<const std::byte> contents) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::remove(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<std::vector<FileInfo>> Client::list(std::string directory) {
	auto request_id = next_id();

	ListRequest request{
		.path = directory,
		.context = {
			.request_id = request_id,
			.sender = NodeIdentity{ self_.node_id, self_.role },
		}
	};

	auto frame = ClientProtocol::encode_list_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive();
	auto rpc_message = Rpc::read_message<ClientJob>(message);

	if (rpc_message.rpc_header.request_id != request_id) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.job != ClientJob::LIST) return std::unexpected(ClientError::BadResponse);

	return ClientProtocol::decode_list_response(rpc_message).info_vec;
}

ClientOperation<void> Client::mkdir(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
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
