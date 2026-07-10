#ifndef NODEINFO_HPP
#define NODEINFO_HPP
#include "cluster/ClusterConfig.hpp"
#include <string>
#include <chrono>

enum class NodeRole {
	Leader,
	Follower,
};

struct NodeInfo {
	std::string node_id;
	std::string ip_address;
	unsigned int port;
	NodeRole role = NodeRole::Follower;

	std::chrono::time_point<std::chrono::system_clock> last_heartbeat;
	// NodeStatus status

	NodeInfo(const ClusterConfig &cfg) : node_id(cfg.node_id), ip_address(cfg.host), port(cfg.port) {}
	NodeInfo() {}
};

#endif // NODEINFO_HPP