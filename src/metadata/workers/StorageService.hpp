#ifndef STORAGE_SERVICE_HPP
#define STORAGE_SERVICE_HPP
#include <variant>
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"

using StorageEvent = std::variant<StorageRpcMessage>;

class StorageService {
  public:
	void handle(StorageRpcMessage msg); // placegholder

	StorageService(const NodeConfig &config, Network<NodeRole::METADATA> &network)
		: config_(config),
		  network_(network) {}

  private:
	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
};

#endif // STORAGE_SERVICE_HPP