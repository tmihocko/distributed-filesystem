#ifndef CLIENT_SERVICE_HPP
#define CLIENT_SERVICE_HPP
#include "MailboxService.hpp"
#include "MetadataStore.hpp"
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
		// Compute obj_id
		// Find replica locations

		FileMetadata metadata{
			.path = req.path,
			.size = 0,
			.obj_id = "hash",				 // FIX!
			.replica_locations = { "", "" }, // Find
		};

		if (!store_.add_metadata(req.path, metadata)) {
			// send_server_error(node_id, request_id);
			// return;
		}
	}

	ClientService(
		const NodeConfig &config,
		Network<NodeRole::METADATA> &network,
		StorageService &storage, MetadataStore &store)
		: config_(config),
		  network_(network),
		  storage_(storage),
		  store_(store) {}

  private:
	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
	StorageService &storage_;
	MetadataStore &store_;
	// inject other services?
};

#endif // CLIENT_SERVICE_HPP