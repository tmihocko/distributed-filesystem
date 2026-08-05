#ifndef NODECONFIG_HPP
#define NODECONFIG_HPP
#include <cstdint>
#include <string>
#include <vector>

using NodeId = std::string;

struct Endpoint {
	NodeId node_id;
	std::string host;
	std::uint16_t port = 0;

	// Returns a stable "host:port" string used for maps and deduplication.
	std::string key() const;
};

namespace Node {

Endpoint get_node_info(const std::string &filename);
std::vector<Endpoint> get_seed_nodes(const std::string &filename);

} // namespace Node

#endif // NODECONFIG_HPP