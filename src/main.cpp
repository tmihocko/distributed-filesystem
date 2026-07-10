#include <asio.hpp>
#include "cluster/ClusterConfig.hpp"
#include "cluster/NodeInfo.hpp"
#include "network/Network.hpp"

int main(int argc, char *argv[]) {
	assert(argc == 2);
	std::string config_file_path = argv[1];
	std::unordered_map<std::string, NodeInfo> peers;

	ClusterConfig config = load_config(config_file_path);
	NodeInfo self_info(config);

	Network::shared().set_address(config.host, config.port);

	std::optional<NodeInfo> leader; // = ClusterService::get().find_leader();
	if (!leader) {
		self_info.role = NodeRole::Leader;
	}

	for (auto ip_addr : config.seed_nodes) {
		// Say hi and ask for other peers and their info

		NodeInfo peer; // = something
		peers.emplace(peer.node_id, peer);
		// if node is leader {
		leader = peer;
		// }
	}

	system("pause");
}