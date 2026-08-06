#include "Client.hpp"
#include "ClientTransport.hpp"
#include "network/Packet.hpp"
#include "protocol/ClientProtocol.hpp"
#include <expected>

ClientOperation<void> Client::connect(const std::vector<Endpoint> &seed_nodes) {
	auto result = transport_.connect(seed_nodes);

	if (!result) {
		return std::unexpected(ClientError::ServerError);
	}

	return {};
}

ClientOperation<void> Client::create_file(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
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
	using namespace ClientProtocol;

	PacketWriter writer;
	writer.write_string(path);

	auto response = transport_.request(Operation::MKDIR, writer.move_data());

	if (!response) return std::unexpected(ClientError::ServerError);

	switch (response->status) {
	case RpcStatus::OK:
		return {};
	case RpcStatus::ALREADY_EXISTS:
		return std::unexpected(ClientError::AlreadyExists);
	default:
		return std::unexpected(ClientError::ServerError);
	}
}

ClientOperation<void> Client::rename(std::string old_path, std::string new_path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<FileInfo> Client::stat(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}