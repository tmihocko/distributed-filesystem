#ifndef CLIENT_SERVICE_HPP
#define CLIENT_SERVICE_HPP
#include "MailboxService.hpp"
#include "StorageService.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <variant>

using ClientEvent = std::variant<CreateFileRequest, Stop>;

class ClientService : public MailboxService<ClientEvent, ClientService> {
  public:
	void handle(CreateFileRequest req) {
		// do_stuff(path);

		// ClientProtocol
	}

	ClientService(
		const NodeConfig &config,
		Network<NodeRole::METADATA> &network,
		StorageService &storage)
		: config_(config),
		  network_(network),
		  storage_(storage) {}

  private:
	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
	StorageService &storage_;
	// inject other services?
};

#endif // CLIENT_SERVICE_HPP