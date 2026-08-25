#ifndef STORAGE_SERVICE_HPP
#define STORAGE_SERVICE_HPP
#include <variant>
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"
#include "MailboxService.hpp"

using StorageEvent = std::variant<StorageRpcMessage, Stop>;

class StorageService : public MailboxService<StorageEvent, StorageService> {
  public:
	StorageService(
		const NodeConfig &config,
		Network<NodeRole::METADATA> &network)
		: config_(config),
		  network_(network) {}

  private:
	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
};

#endif // STORAGE_SERVICE_HPP