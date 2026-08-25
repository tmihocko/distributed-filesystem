#ifndef NODE_HPP
#define NODE_HPP
#include <cstdint>
#include <string>

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
};

struct NodeConfig {
	NodeId node_id;
	NodeRole role = NodeRole::CLIENT;
	std::string host;
	std::uint16_t port = 0;
	std::string data_directory;
};

struct NodeIdentity {
	NodeId id;
	NodeRole role;

	bool operator==(const NodeIdentity &) const = default;
};

#endif // NODE_HPP