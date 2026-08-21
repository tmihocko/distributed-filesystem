#include "Client.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "network/Packet.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <cstdint>
#include <expected>

Client::Client(Endpoint self, std::span<Endpoint> seed_nodes) : network_(self) {
	network_.start(seed_nodes);
}

ClientOperation<void> Client::create_file(std::string path) {
	PacketWriter writer{ path };

	auto id = next_id();

	Frame frame = Rpc::make_frame(id, ClientJob::CREATE_FILE, RpcKind::Request, writer.move_data());

	if (leader_) {
		network_.send(*leader_, frame);
	} else {
		// Maybe make Network::send_to_one(NodeRole, frame);
		network_.broadcast(NodeRole::METADATA, frame);
	}

	// This should run syncronously, since client side is single-threaded, we can just receive the next i believe
	// Double check
	Message response = network_.receive();

	if (response.header.type != MessageType::CLIENT_RPC) return std::unexpected(ClientError::BadResponse);

	RpcMessage<ClientJob> rpc_message = Rpc::read_message<ClientJob>(response);

	// reading should be in generalized rpc, maybe?
	PacketReader reader{ rpc_message.body };

	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.request_id != id) return std::unexpected(ClientError::BadResponse);

	if (rpc_message.rpc_header.job == ClientJob::LEADER_HINT) { // Is this correct usage of header.job, maybe we use bit to show
		leader_ = reader.read_string();
		return create_file(path); // This might not be the best course of action, return an unexpected? wont matter much though
	} else {
		bool success = reader.read<std::uint8_t>() == 1;
		if (!success) {
			return std::unexpected(ClientError::ServerError);
		} else {
			return {};
		}
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
