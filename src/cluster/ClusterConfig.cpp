#include "ClusterConfig.hpp"
#include "yaml-cpp/node/parse.h"
#include <iostream>
#include <yaml-cpp/yaml.h>

ClusterConfig load_config(const std::string &filename) {
	ClusterConfig config;
	// Parse yaml or toml or whatever
	try {
		YAML::Node yaml = YAML::LoadFile(filename);

		config.node_id = yaml["node"]["node_id"].as<std::string>();
		config.host = yaml["node"]["host"].as<std::string>();
		config.port = yaml["node"]["port"].as<unsigned int>();

		for (const auto &seed_node : yaml["seed_nodes"]) {
			config.seed_nodes.push_back(seed_node.as<std::string>());
		}

	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return config;
}