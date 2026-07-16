#include "NodeConfig.hpp"
#include "yaml-cpp/exceptions.h"
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/node/parse.h>

std::string Endpoint::key() const {
	return host + ":" + std::to_string(port);
}

NodeInfo get_node_info(const std::string &filename) {
	NodeInfo info;

	try {
		YAML::Node yaml = YAML::LoadFile(filename);

		info.endpoint.node_id = yaml["node"]["node_id"].as<std::string>();
		info.endpoint.host = yaml["node"]["host"].as<std::string>();
		info.endpoint.port = yaml["node"]["port"].as<std::uint16_t>();

	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return info;
} // namespace get_node_info(conststd::string

std::vector<NodeInfo> get_seed_nodes(const std::string &filename) {
	std::vector<NodeInfo> nodes;

	try {
		YAML::Node yaml = YAML::LoadFile(filename);
		for (const auto &item : yaml["seed_nodes"]) {

			auto node_id = item["node_id"].as<std::string>();
			auto host = item["host"].as<std::string>();
			auto port = item["port"].as<std::uint16_t>();

			nodes.emplace_back(Endpoint{ node_id, host, port }, NodeRole::FOLLOWER);
		}
	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return nodes;
}
