#include "ClientRPC.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "network/Packet.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <expected>

Client::Client(Endpoint self, std::span<Endpoint> seed_nodes) : network_(self) {
	network_.start(seed_nodes);
}

ClientOperation<void> Client::create_file(std::string path) {
	PacketWriter writer{ path };

	Frame frame = make_frame<ClientJob::CREATE_FILE>(writer.move_data());

	if (leader_) {
		network_.send(*leader_, frame);
	} else {
		network_.broadcast(NodeRole::METADATA, frame);
	}

	// Wait for response synchronously
	// return after its finished or times out
	return {};
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

template <ClientJob Job>
Frame Client::make_frame(std::span<const std::byte> buffer) {
	Frame frame;
	PacketWriter writer;

	ClientRpcHeader rpc_header{
		.request_id = 0x00, // change to next_id()
		.job = Job,
		.kind = RpcKind::Request,
	};

	writer
		.write(rpc_header) // Add like the client RPC stuff here
		.write_bytes(buffer);

	frame.header = MessageHeader{
		.magic = HEADER_MAGIC,
		.length = writer.length(),
		.type = MessageType::CLIENT_RPC,
	};

	frame.buffer = writer.move_data();

	return frame;
}