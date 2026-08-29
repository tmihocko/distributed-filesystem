/**
Does all jobs given to metadata node
*/
#ifndef METADATA_WORKER_HPP
#define METADATA_WORKER_HPP
#include "MetadataStore.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "ObjectId.hpp"
#include "rpc/StorageProtocol.hpp"

struct PendingWrite {
	std::string path;
	std::string object_id;
	std::uint64_t file_size;
	std::uint32_t chunk_count;
	std::uint32_t next_chunk;
	std::array<NodeId, 2> replicas;
};

using ClientEvent = std::variant<CreateFileRequest, WriteFileRequest, WriteChunkRequest, ListRequest, MakeDirRequest>;

class MetadataWorker {
  public:
	// Metadata only operation
	void handle(CreateFileRequest req) {
		FileMetadata metadata{
			.path = req.path,
			.size = 0,
			.obj_id = ObjectId::generate(),
			.replica_locations = { "", "" }, // Leave empty, find on write,
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

	void handle(WriteFileRequest req) {
		auto respond = [&](ClientStatus status) {
			Frame frame = ClientProtocol::encode_write_file_response(
				req.context.request_id,
				WriteFileResponse{
					.status = status,
					.context = req.context,
				});

			network_.send(req.context.sender.id, std::move(frame));
		};
		const std::uint64_t expected_chunks = req.file_size / CHUNK_SIZE + (req.file_size % CHUNK_SIZE != 0);

		if (expected_chunks != req.chunk_count) {
			respond(ClientStatus::ServerError);
			return;
		}

		auto metadata_result = store_.get_metadata(req.path);

		if (!metadata_result && metadata_result.error() != MetadataStoreError::NOT_FOUND) {
			respond(ClientStatus::ServerError); // Currently requires create_file to be called first, change this later
			return;
		}

		FileMetadata metadata = metadata_result ? std::move(*metadata_result) : FileMetadata{
			.path = req.path,
			.size = 0,
			.obj_id = ObjectId::generate(),
			.replica_locations = { "", "" },
		};

		if (metadata.obj_id.empty()) {
			respond(ClientStatus::ServerError); // Corrupt metadata: CREATE_FILE should assign this.
			return;
		}

		std::array<NodeId, 2> replicas = metadata.replica_locations;

		const bool first_bound = !replicas[0].empty();
		const bool second_bound = !replicas[1].empty();

		if (first_bound != second_bound) { // One empty one full, not allowed
			respond(ClientStatus::ServerError);
			return;
		}

		if (first_bound && replicas[0] == replicas[1]) { // Node has both replicas
			respond(ClientStatus::ServerError);
			return;
		}

		if (req.chunk_count == 0) {
			metadata.size = 0;

			const auto update_result = store_.update_metadata(req.path, std::move(metadata));

			respond(update_result.has_value() ? ClientStatus::Success : ClientStatus::ServerError);
			return;
		}

		if (first_bound) {
			for (const NodeId &replica : replicas) {
				auto node_it = storage_nodes_.find(replica);

				if (node_it == storage_nodes_.end() || !node_it->second.available) {
					respond(ClientStatus::ServerError);
					return;
				}
			}
		} else {
			std::vector<NodeId> candidates;

			for (const auto &[node_id, info] : storage_nodes_) {
				if (info.available) {
					candidates.push_back(node_id);
				}
			}

			std::sort(
				candidates.begin(),
				candidates.end(),
				[this](const NodeId &lhs, const NodeId &rhs) {
					const auto &left = storage_nodes_.at(lhs);
					const auto &right = storage_nodes_.at(rhs);

					if (left.bytes_left != right.bytes_left) {
						return left.bytes_left > right.bytes_left;
					}

					return lhs < rhs;
				});

			if (candidates.size() < 2) {
				respond(ClientStatus::ServerError);
				return;
			}

			replicas = { candidates[0], candidates[1] };

			metadata.replica_locations = replicas;
			const auto bind_result = store_.update_metadata(req.path, metadata);

			if (!bind_result) {
				respond(ClientStatus::ServerError);
				return;
			}
		}

		auto &client_writes = pending_writes_[req.context.sender.id];

		const auto [_, inserted] = client_writes.try_emplace(
			req.context.request_id,
			PendingWrite{
				.path = std::move(req.path),
				.object_id =
					std::move(metadata.obj_id),
				.file_size = req.file_size,
				.chunk_count = req.chunk_count,
				.next_chunk = 0,
				.replicas = replicas,
			});

		respond(inserted ? ClientStatus::Success : ClientStatus::ServerError);
	}

	void handle(WriteChunkRequest req) {
		auto respond = [&](ClientStatus status) {
			Frame frame = ClientProtocol::encode_write_chunk_response(
				req.context.request_id,
				WriteChunkResponse{
					.chunk_index = req.chunk_index,
					.status = status,
					.context = req.context,
				});

			network_.send(req.context.sender.id, std::move(frame));
		};

		auto client_it = pending_writes_.find(req.context.sender.id);
		if (client_it == pending_writes_.end()) {
			respond(ClientStatus::InputError);
			return;
		}

		auto write_it = client_it->second.find(req.context.request_id);
		if (write_it == client_it->second.end()) {
			respond(ClientStatus::InputError);
			return;
		}

		PendingWrite &write = write_it->second;
		auto discard_write = [&] {
			client_it->second.erase(write_it);

			if (client_it->second.empty()) {
				pending_writes_.erase(client_it);
			}
		};

		if (req.chunk_index != write.next_chunk || req.chunk_index >= write.chunk_count) {
			discard_write();
			respond(ClientStatus::InputError);
			return;
		}

		const std::uint64_t offset = static_cast<std::uint64_t>(req.chunk_index) * CHUNK_SIZE;
		const std::uint64_t remaining = write.file_size - offset;
		const std::size_t expected_size = static_cast<std::size_t>(std::min<std::uint64_t>(CHUNK_SIZE, remaining));

		if (req.data.size() != expected_size) {
			discard_write();
			respond(ClientStatus::InputError);
			return;
		}

		PutRequest put{
			.object_id = write.object_id,
			.chunk_index = req.chunk_index,
			.final_chunk = req.chunk_index + 1 == write.chunk_count,
			.data = req.data,
		};

		// Send both replicas before waiting so their writes can
		// happen concurrently.
		for (const NodeId &replica : write.replicas) {
			network_.send(replica, StorageProtocol::encode_put_request(req.context.request_id, put));
		}

		bool replicas_succeeded = true;

		for (const auto &replica : write.replicas) {
			auto response_message = network_.receive_if(storage_request_timeout_, [&](const Message &message) {
				if (message.sender.id != replica || !Rpc::message_is<StorageJob, RpcKind::Response>(message)) {
					return false;
				}

				try {
					BinaryReader reader{ message.buffer };

					const auto [request_id, job, kind, chunk_index] =
						reader.read<std::uint64_t, StorageJob, RpcKind, std::uint32_t>();

					return request_id == req.context.request_id &&
						   job == StorageJob::PUT &&
						   kind == RpcKind::Response &&
						   chunk_index == req.chunk_index;
				} catch (const std::exception &) {
					return false;
				}
			});

			if (!response_message) {
				replicas_succeeded = false;
				return;
			}

			const auto rpc_response = Rpc::read_message<StorageJob>(std::move(*response_message));
			const PutResponse response = StorageProtocol::decode_put_response(rpc_response);

			if (response.status != StorageStatus::Success) {
				replicas_succeeded = false;
			}
		}

		if (!replicas_succeeded) {
			discard_write();
			respond(ClientStatus::ServerError);
			return;
		}

		write.next_chunk++;

		const bool finished = write.next_chunk == write.chunk_count;

		if (!finished) { // finished
			// create the file in metadata here
			respond(ClientStatus::Success);
			return;
		}

		const auto metadata_result = store_.update_metadata(
			write.path,
			FileMetadata{
				.path = write.path,
				.size = write.file_size,
				.obj_id = write.object_id,
				.replica_locations = write.replicas,
			});

		const ClientStatus final_status = get_client_status(metadata_result);

		discard_write(); // Not really discard, more like destruct
		respond(final_status);
	}

	void handle(ListRequest req) {
		auto expected = store_.list(req.path);
		ClientStatus status = get_client_status(expected);

		Frame frame;

		if (status == ClientStatus::Success) {
			frame = ClientProtocol::encode_list_response(
				req.context.request_id,
				ListResponse{
					.status = ClientStatus::Success,
					.info_vec = std::move(expected.value()),
				});
		} else {
			frame = ClientProtocol::encode_list_response(
				req.context.request_id,
				ListResponse{
					// Doesnt support optionals :(
					.status = ClientStatus::ServerError,
					.info_vec = std::vector<FileInfo>(0),
				});
		}

		network_.send(req.context.sender.id, std::move(frame));
	}

	void handle(MakeDirRequest req) {
		auto expected = store_.make_directory(req.path);
		ClientStatus status = get_client_status(expected);

		Frame frame = ClientProtocol::encode_mdkir_response(
			req.context.request_id,
			MakeDirResponse{
				.status = status,
				.context = req.context,
			});

		network_.send(req.context.sender.id, std::move(frame));
	}

	MetadataWorker(const NodeConfig &config, Network<NodeRole::METADATA> &network, MetadataStore &store, std::unordered_map<NodeId, StorageNodeInfo> &nodes)
		: config_(config),
		  network_(network),
		  store_(store),
		  storage_nodes_(nodes) {}

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
	MetadataStore &store_;
	std::unordered_map<NodeId, StorageNodeInfo> &storage_nodes_;

	// { ClientID : { RequestId : PendingWrite } }
	std::unordered_map<NodeId, std::unordered_map<std::uint64_t, PendingWrite>> pending_writes_;
	std::chrono::seconds storage_request_timeout_{ 3 };
};

#endif // METADATA_WORKER_HPP