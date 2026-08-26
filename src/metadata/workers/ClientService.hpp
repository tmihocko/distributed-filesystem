#ifndef CLIENT_SERVICE_HPP
#define CLIENT_SERVICE_HPP
#include "MetadataStore.hpp"
#include "workers/StorageService.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"

using ClientEvent = std::variant<CreateFileRequest>;

class ClientService {
  public:
	// Metadata only operation
	void handle(CreateFileRequest req) {
		FileMetadata metadata{
			.path = req.path,
			.size = 0,
			.obj_id = "hash",				 // FIX!
			.replica_locations = { "", "" }, // Find
		};

		auto expected = store_.add_metadata(req.path, metadata);
		ClientStatus status;

		if (expected) {
			status = ClientStatus::Success;
		} else {
			switch (expected.error()) {
			case MetadataStoreError::INVALID_PATH:
				status = ClientStatus::InputError;
				break;
			default:
				status = ClientStatus::ServerError;
			}
		}

		Frame frame = ClientProtocol::encode_create_file_response(
			req.context.request_id,
			CreateFileResponse{
				.status = status,
				.context = req.context,
			});

		network_.send(req.context.sender.id, std::move(frame));
	}

	void handle();

	ClientService(const NodeConfig &config, Network<NodeRole::METADATA> &network, StorageService &storage, MetadataStore &store)
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