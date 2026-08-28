/**

TODO: leader_ is still here after removing raft, store metadata address/id and use that instead
*/
#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include <expected>
#include <span>
#include <string>
#include <vector>
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"

template <typename T>
using ClientOperation = std::expected<T, ClientError>;

class Client {
  public:
	[[nodiscard]] Client(Endpoint self, std::span<const Endpoint> seed_nodes);
	[[nodiscard]] Client(const std::string &config_file);

	// ClientOperation<void> connect(const std::vector<Endpoint> &seed_nodes);

	ClientOperation<void> create_file(std::string path);

	[[nodiscard]]
	ClientOperation<std::vector<std::byte>> read_file(std::string path, std::size_t byte_count);

	ClientOperation<void> write_file(std::string local_path, std::string path);

	ClientOperation<void> remove(std::string path);

	ClientOperation<std::vector<FileInfo>> list(std::string directory = "/");

	ClientOperation<void> mkdir(std::string path);

	ClientOperation<void> rename(std::string old_path, std::string new_path);

	[[nodiscard]]
	ClientOperation<FileStats> stat(std::string path);

	Client(const Client &) = delete;
	auto operator=(const Client &) = delete;
	Client(Client &&) = delete;
	auto operator=(Client &&) = delete;

  private:
	template <ClientJob Job, RpcKind Kind = RpcKind::Response>
	bool validate_rpc_header(RpcHeader<ClientJob> header, std::uint64_t request_id) {
		return header.request_id == request_id &&
			   header.kind == Kind &&
			   header.job == Job;
	}

	RequestContext request_context(std::uint64_t request_id);

	template <typename T>
	std::expected<T, ClientError> status_to_error(ClientStatus status) {
		switch (status) {
		case ClientStatus::AlreadyExists:
			return std::unexpected(ClientError::AlreadyExists);
		default:
			return std::unexpected(ClientError::ServerError);
		}
	}

	std::uint64_t next_id();
	std::uint64_t current_id_ = 0;

	Endpoint self_;
	Network<NodeRole::CLIENT> network_;

	NodeId metadata_node_id_;
};

#endif // CLIENT_HPP