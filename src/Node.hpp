#ifndef NODECONFIG_HPP
#define NODECONFIG_HPP
#include <cstdint>
#include <string>
#include <vector>

using NodeId = std::string;
using Term = std::uint64_t;

enum class NodeRole : std::uint8_t {
	FOLLOWER = 0,
	LEADER = 1,

};

struct Endpoint {
	NodeId node_id;
	std::string host;
	std::uint16_t port = 0;

	// Returns a stable "host:port" string used for maps and deduplication.
	std::string key() const;
};

struct NodeInfo {
	Endpoint endpoint;
	NodeRole role;
};

NodeInfo get_node_info(const std::string &filename);

std::vector<NodeInfo> get_seed_nodes(const std::string &filename);

#endif // NODECONFIG_HPP