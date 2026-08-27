#ifndef CLIENT_SERVICE_HPP
#define CLIENT_SERVICE_HPP
#include "MetadataStore.hpp"
#include "workers/StorageService.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"

using ClientEvent = std::variant<CreateFileRequest, ListRequest>;

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
		ClientStatus status = get_client_status(expected);

		Frame frame = ClientProtocol::encode_create_file_response(
			req.context.request_id,
			CreateFileResponse{
				.status = status,
				.context = req.context,
			});

		network_.send(req.context.sender.id, std::move(frame));
	}

	void handle(ListRequest req) {
		auto expected = store_.list(req.path);
		ClientStatus status = get_client_status(expected);

		Frame frame;

		if (status == ClientStatus::Success) {
			frame = ClientProtocol::encode_list_response(
				req.context.request_id,
				ListResponse{
					.info_vec = std::move(expected.value()),
				});
		} else {
			frame = ClientProtocol::encode_list_response(
				req.context.request_id,
				ListResponse{
					// Unused empty vector, need this because i dont want rpc stuff to be super verbose
					.info_vec = std::vector<FileInfo>(0),
				});
		}

		network_.send(req.context.sender.id, std::move(frame));
	}

	ClientService(const NodeConfig &config, Network<NodeRole::METADATA> &network, StorageService &storage, MetadataStore &store)
		: config_(config),
		  network_(network),
		  storage_(storage),
		  store_(store) {}

  private:
	template <typename T>
	ClientStatus get_client_status(const std::expected<T, MetadataStoreError> &expected) {
		if (expected) {
			return ClientStatus::Success;
		} else {
			switch (expected.error()) {
			case MetadataStoreError::INVALID_PATH:
				return ClientStatus::InputError;
			case MetadataStoreError::ALREADY_EXISTS:
				return ClientStatus::AlreadyExists;
			default:
				return ClientStatus::ServerError;
			}
		}
	}

	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
	StorageService &storage_;
	MetadataStore &store_;
	// inject other services?
};

#endif // CLIENT_SERVICE_HPP