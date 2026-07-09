#ifndef CLUSTERCONFIG_HPP
#define CLUSTERCONFIG_HPP
#include <string>
#include <vector>

struct ClusterConfig {
	std::string node_id;
	std::string host;
	unsigned int port;
	std::vector<std::string> seed_nodes; // This might need port as well
};

ClusterConfig load_config(const std::string &file_path);

#endif // CLUSTERCONFIG_HPP