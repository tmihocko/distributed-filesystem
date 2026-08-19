#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "network/Node.hpp"
#include "util/Singleton.hpp"
#include <expected>
#include <span>
#include <string>
#include <vector>

enum ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
	NotImplemented,
};

struct FileInfo {
	std::string path;
	bool is_directory = false;
	std::uint64_t size = 0;
};

template <typename T>
using ClientOperation = std::expected<T, ClientError>;

class Client final : public Singleton<Client> {
  public:
	ClientOperation<void> connect(const std::vector<Endpoint> &seed_nodes);

	ClientOperation<void> create_file(std::string path);

	ClientOperation<std::vector<std::byte>> read_file(std::string path);

	ClientOperation<void> write_file(std::string path, std::span<const std::byte> contents);

	ClientOperation<void> remove(std::string path);

	ClientOperation<std::vector<FileInfo>> list(std::string path = "/");

	ClientOperation<void> mkdir(std::string path);

	ClientOperation<void> rename(std::string old_path, std::string new_path);

	ClientOperation<FileInfo> stat(std::string path);

  private:
};

#endif // CLIENT_HPP