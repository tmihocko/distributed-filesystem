#include "Node.hpp"
#include "yaml-cpp/exceptions.h"
#include <cctype>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <yaml-cpp/node/parse.h>

constexpr NodeRole role_from_string(std::string str) {
	if (str == "Client") {
		return NodeRole::CLIENT;
	} else if (str == "Metadata") {
		return NodeRole::METADATA;
	} else if (str == "Storage") {
		return NodeRole::STORAGE;
	} else {
		throw std::invalid_argument("Got input: " + str);
	}
}

std::string Endpoint::key() const {
	return host + ":" + std::to_string(port);
}

Endpoint Node::get_node_info(const std::string &filename) {
	Endpoint info;

	try {
		YAML::Node yaml = YAML::LoadFile(filename);

		info.node_id = yaml["node"]["node_id"].as<std::string>();
		info.role = role_from_string(yaml["node"]["role"].as<std::string>());
		info.host = yaml["node"]["host"].as<std::string>();
		info.port = yaml["node"]["port"].as<std::uint16_t>();

	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return info;
} // namespace get_node_info(conststd::string

std::vector<Endpoint> Node::get_seed_nodes(const std::string &filename) {
	std::vector<Endpoint> nodes;

	try {
		YAML::Node yaml = YAML::LoadFile(filename);
		for (const auto &item : yaml["seed_nodes"]) {

			auto node_id = item["node_id"].as<std::string>();
			auto role = role_from_string(item["role"].as<std::string>());
			auto host = item["host"].as<std::string>();
			auto port = item["port"].as<std::uint16_t>();

			nodes.emplace_back(Endpoint{ node_id, role, host, port });
		}
	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return nodes;
}
