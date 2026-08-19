#include "Client.hpp"
#include <expected>

ClientOperation<void> Client::connect(const std::vector<Endpoint> &seed_nodes) {
	return std::unexpected(ClientError::NotImplemented);
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
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<void> Client::rename(std::string old_path, std::string new_path) {
	return std::unexpected(ClientError::NotImplemented);
}

ClientOperation<FileInfo> Client::stat(std::string path) {
	return std::unexpected(ClientError::NotImplemented);
}