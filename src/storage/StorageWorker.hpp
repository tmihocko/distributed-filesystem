#ifndef STORAGEWORKER_HPP
#define STORAGEWORKER_HPP
#include <filesystem>
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"

class StorageWorker {
  public:
	void handle(PutRequest req);
	void handle(GetRequest req);
	void handle(DeleteRequest req);

	StorageWorker(const NodeConfig &config, Network<NodeRole::STORAGE> &network);

  private:
	bool valid_object_id(const std::string &object_id);
	StorageStatus write_chunk(const PutRequest &req);

	void abort_write(std::string_view object_id, std::filesystem::path path);

	std::uint64_t available_bytes();

	std::filesystem::path objects_directory_;
	NodeConfig config_;
	Network<NodeRole::STORAGE> &network_;

	std::unordered_map<std::string, std::uint32_t> next_chunks_;
}; // Does worker jobs

#endif // STORAGEWORKER_HPP