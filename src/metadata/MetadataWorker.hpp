/**
Does all jobs given to metadata node
*/
#ifndef METADATA_WORKER_HPP
#define METADATA_WORKER_HPP
#include "MetadataStore.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"

struct PendingRead {
	std::string path;
	std::string object_id;
	std::uint64_t file_size;
	std::uint32_t chunk_count;
	std::uint32_t next_chunk;
	std::array<NodeId, 2> replicas;
};

struct PendingWrite {
	std::string path;
	std::string object_id;
	std::uint64_t file_size;
	std::uint32_t chunk_count;
	std::uint32_t next_chunk;
	std::array<NodeId, 2> replicas;
};

using ClientEvent =
	std::variant<CreateFileRequest, ReadFileRequest, ReadChunkRequest,
				 WriteFileRequest, WriteChunkRequest, RemoveRequest,
				 ListRequest, MakeDirRequest, RenameRequest,
				 StatRequest>;

class MetadataWorker {

  public:
	// Metadata only operation
	void handle(CreateFileRequest req);

	void handle(WriteFileRequest req);

	void handle(WriteChunkRequest req);

	void handle(ReadFileRequest req);

	void handle(ReadChunkRequest req);

	void handle(RemoveRequest req);

	void handle(ListRequest req);

	void handle(MakeDirRequest req);

	void handle(RenameRequest req);

	void handle(StatRequest req);

	MetadataWorker(const NodeConfig &config, Network<NodeRole::METADATA> &network, MetadataStore &store, std::unordered_map<NodeId, StorageNodeInfo> &nodes)
		: config_(config),
		  network_(network),
		  store_(store),
		  storage_nodes_(nodes) {}

  private:
	template <typename Response, typename Encoder>
	auto make_responder(RequestContext context, Response response, Encoder encoder) {
		return [this, context = std::move(context), response = std::move(response), encoder](auto status) mutable {
			response.status = status;

			Frame frame = encoder(context.request_id, response);

			network_.send(context.sender.id, std::move(frame));
		};
	}

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

	std::uint64_t next_storage_request_id() {
		return ++current_storage_request_id_;
	}

	std::uint64_t current_storage_request_id_ = 0;

	NodeConfig config_;
	Network<NodeRole::METADATA> &network_;
	MetadataStore &store_;
	std::unordered_map<NodeId, StorageNodeInfo> &storage_nodes_;

	// { ClientID : { RequestId : PendingWrite } }
	std::unordered_map<NodeId, std::unordered_map<std::uint64_t, PendingRead>> pending_reads_;
	std::unordered_map<NodeId, std::unordered_map<std::uint64_t, PendingWrite>> pending_writes_;
	std::chrono::seconds storage_request_timeout_{ 3 };
};

#endif // METADATA_WORKER_HPP