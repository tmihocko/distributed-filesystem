#ifndef NODECONFIG_HPP
#define NODECONFIG_HPP
#include <cstdint>
#include <string>
#include <vector>

using NodeId = std::string;

enum class NodeRole : std::uint8_t {
	CLIENT,
	METADATA,
	STORAGE
};

constexpr std::uint8_t role_bit(NodeRole role) {
	return 1u << static_cast<std::uint8_t>(role);
}

template <NodeRole Role>
inline constexpr std::uint8_t PeerRoles = 0;

template <>
inline constexpr std::uint8_t PeerRoles<NodeRole::METADATA> =
	role_bit(NodeRole::METADATA) |
	role_bit(NodeRole::STORAGE) |
	role_bit(NodeRole::CLIENT);

template <>
inline constexpr std::uint8_t PeerRoles<NodeRole::STORAGE> =
	role_bit(NodeRole::METADATA);

template <>
inline constexpr std::uint8_t PeerRoles<NodeRole::CLIENT> =
	role_bit(NodeRole::METADATA);

struct Endpoint {
	NodeId node_id;
	NodeRole role = NodeRole::CLIENT;
	std::string host;
	std::uint16_t port = 0;

	// Returns a stable "host:port" string used for maps and deduplication.
	std::string key() const;
};

struct NodeIdentity {
	NodeId id;
	NodeRole role;

	bool operator==(const NodeIdentity &) const = default;
};

namespace Node {

Endpoint get_node_info(const std::string &filename);
std::vector<Endpoint> get_seed_nodes(const std::string &filename);

} // namespace Node

#endif // NODECONFIG_HPP