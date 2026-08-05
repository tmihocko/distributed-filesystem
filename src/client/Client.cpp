#include "Client.hpp"
#include "protocol/ClientProtocol.hpp"
#include "ClientTransport.hpp"
#include "network/Packet.hpp"
#include "protocol/ClientProtocol.hpp"
#include <expected>

std::expected<void, ClientError> Client::connect(const std::vector<Endpoint> &seed_nodes) {
	auto result = transport_.connect(seed_nodes);

	if (!result) {
		return std::unexpected(ClientError::ServerError);
	}

	return {};
}

std::expected<void, ClientError> Client::mkdir(std::string path) {
	using namespace ClientProtocol;

	PacketWriter writer;
	writer.write_string(path);

	auto response = transport_.request(Operation::MKDIR, writer.move_data());

	if (!response) {
		return std::unexpected(ClientError::ServerError);
	}

	switch (response->status) {
	case RpcStatus::OK:
		return {};
	case RpcStatus::ALREADY_EXISTS:
		return std::unexpected(ClientError::AlreadyExists);
	default:
		return std::unexpected(ClientError::ServerError);
	}

	return {};
}