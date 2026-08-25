#include "Yaml.hpp"
#include <iostream>
#include <string>
#include "network/Node.hpp"
#include "yaml-cpp/yaml.h"

static NodeRole role_from_string(std::string str) {
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

Endpoint Yaml::get_node_info(const std::string &filename) {
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
}

NodeConfig Yaml::get_node_config(const std::string &filename) {
	NodeConfig config;

	try {
		YAML::Node yaml = YAML::LoadFile(filename);

		config.node_id = yaml["node"]["node_id"].as<std::string>();
		config.role = role_from_string(yaml["node"]["role"].as<std::string>());
		config.host = yaml["node"]["host"].as<std::string>();
		config.port = yaml["node"]["port"].as<std::uint16_t>();
		config.data_directory = yaml["node"]["data_dir"].as<std::string>();

	} catch (const YAML::Exception &e) {
		std::cerr << "YAML error: " << e.what() << "\n";
	}

	return config;
}

std::vector<Endpoint> Yaml::get_seed_nodes(const std::string &filename) {
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
