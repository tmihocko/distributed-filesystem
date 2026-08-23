#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include <expected>
#include <span>
#include <string>
#include <vector>

enum class ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
	BadResponse,
	NotImplemented,
};

struct FileInfo {
	std::string path;
	bool is_directory = false;
	std::uint64_t size = 0;
};

template <typename T>
using ClientOperation = std::expected<T, ClientError>;

class Client {
  public:
	[[nodiscard]] Client(Endpoint self, std::span<const Endpoint> seed_nodes);
	[[nodiscard]] Client(const std::string &config_file);

	// ClientOperation<void> connect(const std::vector<Endpoint> &seed_nodes);

	ClientOperation<void> create_file(std::string path);

	ClientOperation<std::vector<std::byte>> read_file(std::string path, std::size_t byte_count);

	ClientOperation<void> write_file(std::string path, std::span<const std::byte> contents);

	ClientOperation<void> remove(std::string path);

	ClientOperation<std::vector<FileInfo>> list(std::string path = "/");

	ClientOperation<void> mkdir(std::string path);

	ClientOperation<void> rename(std::string old_path, std::string new_path);

	ClientOperation<FileInfo> stat(std::string path);

	Client(const Client &) = delete;
	auto operator=(const Client &) = delete;
	Client(Client &&) = delete;
	auto operator=(Client &&) = delete;

  private:
	std::uint64_t next_id();
	std::uint64_t current_id_ = 0;

	Endpoint self_;
	Network<NodeRole::CLIENT> network_;

	std::optional<NodeId> leader_;
};

#endif // CLIENT_HPP