#include "Client.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <cstdint>
#include <expected>

Client::Client(Endpoint self, std::span<Endpoint> seed_nodes) : self_(self), network_(self) {
	network_.start(seed_nodes);
}

ClientOperation<void> Client::create_file(std::string path) {

	auto request_id = next_id();

	CreateFileRequest request{
		.path = std::move(path),
		.context = {
			.request_id = request_id,
			.sender = NodeIdentity{ self_.node_id, self_.role } },
	};

	auto frame = ClientProtocol::encode_create_file_request(request_id, request);

	if (leader_) {
		network_.send(*leader_, frame);
	} else {
		// network_.send_to_one_of<NodeRole::Metadata>(frame);
	}

	auto message = network_.receive();
	auto rpc_message = Rpc::read_message<ClientJob>(message);

	if (rpc_message.rpc_header.request_id != request_id) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);

	switch (rpc_message.rpc_header.job) {
	case ClientJob::CREATE_FILE: {
		CreateFileResponse response = ClientProtocol::decode_create_file_response(rpc_message);
		if (response.status != ClientStatus::Success) return std::unexpected(ClientError::ServerError);

		leader_ = rpc_message.sender.id; // Incase our network.send_to_one_of landed on the leader
		return {};
		break;
	}
	// This should be generalized to a update_leader(NodeId new_leader, std::function retry)
	case ClientJob::NOT_LEADER: {
		LeaderHintResponse response = ClientProtocol::decode_leader_hint_response(rpc_message);

		if (!response.leader_id) return std::unexpected(ClientError::ServerError);

		leader_ = std::move(response.leader_id);
		return create_file(path); // retry

		break;
	}
	default:
		throw std::runtime_error("Unexpected job type");
	}

	/**
	Big todos:
	Integer endianness in packetserializers???

	How can we make types explicit at compile time for response and requests of jobs ::(low priority, specialized RPC is still RPC internals)
	ie, make it explicit in client and metadata node, that for CreateFile request a string is sent, for a CreateFile response a bool is sent

	*/
}

ClientOperation<std::vector<std::byte>> Client::read_file(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::write_file(std::string path, std::span<const std::byte> contents) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::remove(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<std::vector<FileInfo>> Client::list(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::mkdir(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::rename(std::string old_path, std::string new_path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<FileInfo> Client::stat(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}

std::uint64_t Client::next_id() {
	return ++current_id_;
}
