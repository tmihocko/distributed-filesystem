#ifndef YAML_HPP
#define YAML_HPP
#include "network/Node.hpp"
#include <vector>

namespace Yaml {

Endpoint get_node_info(const std::string &filename);
std::vector<Endpoint> get_seed_nodes(const std::string &filename);
NodeConfig get_node_config(const std::string &filename);

} // namespace Yaml

#endif // YAML_HPP