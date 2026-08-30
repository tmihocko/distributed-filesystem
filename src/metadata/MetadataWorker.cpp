#include "MetadataWorker.hpp"
#include "rpc/ClientProtocol.hpp"
#include "ObjectId.hpp"
#include "rpc/Rpc.hpp"
#include "rpc/StorageProtocol.hpp"

void MetadataWorker::handle(CreateFileRequest req) {

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

void MetadataWorker::handle(WriteFileRequest req) {
	auto respond = make_responder(req.context, WriteFileResponse{}, ClientProtocol::encode_write_file_response);
	const std::uint64_t expected_chunks = req.file_size / CHUNK_SIZE + (req.file_size % CHUNK_SIZE != 0);

	if (expected_chunks != req.chunk_count) {
		respond(ClientStatus::ServerError);
		return;
	}

	auto metadata_result = store_.get_metadata(req.path);

	if (!metadata_result && metadata_result.error() != MetadataStoreError::NOT_FOUND) {
		respond(ClientStatus::ServerError); // Requires create_file to be called first, double check this later !TODO:
		return;
	}

	FileMetadata metadata = metadata_result ? std::move(*metadata_result) : FileMetadata{
		.path = req.path,
		.size = 0,
		.obj_id = ObjectId::generate(),
		.replica_locations = { "", "" },
	};

	// Corrupt metadata: CREATE_FILE should assign this.
	if (metadata.obj_id.empty()) {
		respond(ClientStatus::ServerError);
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

void MetadataWorker::handle(WriteChunkRequest req) {
	auto respond = make_responder(
		req.context,
		WriteChunkResponse{
			.chunk_index = req.chunk_index,
			.context = req.context,
		},
		ClientProtocol::encode_write_chunk_response);

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
		auto response_message = network_.receive_if(
			storage_request_timeout_,
			Rpc::make_message_is(
				req.context.request_id,
				StorageJob::PUT,
				RpcKind::Response,
				replica,
				[expected_chunk = req.chunk_index](BinaryReader &reader) { return reader.read<std::uint32_t>() == expected_chunk; }));

		if (!response_message) {
			replicas_succeeded = false;
			continue;
		}

		const auto rpc_response = Rpc::read_message<StorageJob>(std::move(*response_message));
		const PutResponse response = StorageProtocol::decode_put_response(rpc_response);

		auto node = storage_nodes_.find(replica);

		if (node != storage_nodes_.end()) {
			node->second.bytes_left = response.size_available;
		}

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

void MetadataWorker::handle(ReadFileRequest req) {
	auto respond_error = make_responder(
		req.context,
		ReadFileResponse{
			.file_size = 0,
			.chunk_count = 0,
			.context = req.context,
		},
		ClientProtocol::encode_read_file_response);

	for (const auto &[client_id, client_writes] : pending_writes_) {
		for (const auto &[request_id, pending_write] : client_writes) {
			if (pending_write.path == req.path) {
				respond_error(ClientStatus::ServerError);
				return;
			}
		}
	}

	auto metadata_result = store_.get_metadata(req.path);

	if (!metadata_result) {
		respond_error(get_client_status(metadata_result));
		return;
	}

	const FileMetadata &metadata = *metadata_result;

	if (metadata.obj_id.empty()) {
		respond_error(ClientStatus::ServerError);
		return;
	}

	const std::uint64_t chunk_count =
		metadata.size / CHUNK_SIZE +
		(metadata.size % CHUNK_SIZE != 0);

	if (chunk_count >
		std::numeric_limits<std::uint32_t>::max()) {
		respond_error(ClientStatus::ServerError);
		return;
	}

	if (metadata.size != 0) {
		const auto &replicas = metadata.replica_locations;

		const bool first_bound = !replicas[0].empty();
		const bool second_bound = !replicas[1].empty();

		if (!first_bound || !second_bound || replicas[0] == replicas[1]) {
			respond_error(ClientStatus::ServerError);
			return;
		}

		bool replica_available = false;

		for (const NodeId &replica : replicas) {
			auto node = storage_nodes_.find(replica);

			if (node != storage_nodes_.end() &&
				node->second.available) {
				replica_available = true;
				break;
			}
		}

		if (!replica_available) {
			respond_error(ClientStatus::ServerError);
			return;
		}
	}

	if (chunk_count != 0) {
		auto &client_reads = pending_reads_[req.context.sender.id];

		const auto [read, inserted] = client_reads.try_emplace(
			req.context.request_id,
			PendingRead{
				.path = req.path,
				.object_id = metadata.obj_id,
				.file_size = metadata.size,
				.chunk_count = static_cast<std::uint32_t>(chunk_count),
				.next_chunk = 0,
				.replicas = metadata.replica_locations,
			});

		if (!inserted) {
			respond_error(ClientStatus::ServerError);
			return;
		}
	}

	const ReadFileResponse response{
		.status = ClientStatus::Success,
		.file_size = metadata.size,
		.chunk_count = static_cast<std::uint32_t>(chunk_count),
		.context = req.context,
	};

	Frame response_frame = ClientProtocol::encode_read_file_response(req.context.request_id, response);

	network_.send(req.context.sender.id, std::move(response_frame));
}

void MetadataWorker::handle(ReadChunkRequest req) {
	auto respond_error = make_responder(
		req.context,
		ReadChunkResponse{
			.chunk_index = req.chunk_index,
			.status = ClientStatus::ServerError,
			.data = {},
			.context = req.context,
		},
		ClientProtocol::encode_read_chunk_response);

	auto client = pending_reads_.find(req.context.sender.id);

	if (client == pending_reads_.end()) {
		respond_error(ClientStatus::InputError);
		return;
	}

	auto read_entry = client->second.find(req.context.request_id);

	if (read_entry == client->second.end()) {
		respond_error(ClientStatus::InputError);
		return;
	}

	PendingRead &read = read_entry->second;

	auto discard_read = [&]() {
		client->second.erase(read_entry);

		if (client->second.empty()) {
			pending_reads_.erase(client);
		}
	};

	if (req.chunk_index != read.next_chunk ||
		req.chunk_index >= read.chunk_count) {
		discard_read();
		respond_error(ClientStatus::InputError);
		return;
	}

	const std::uint64_t offset = static_cast<std::uint64_t>(req.chunk_index) * CHUNK_SIZE;

	const std::uint32_t expected_size = static_cast<std::uint32_t>(std::min<std::uint64_t>(CHUNK_SIZE, read.file_size - offset));

	std::vector<std::byte> chunk_data;
	bool chunk_received = false;

	for (const NodeId &replica : read.replicas) {
		auto storage_node = storage_nodes_.find(replica);

		if (storage_node == storage_nodes_.end() ||
			!storage_node->second.available) {
			continue;
		}

		const std::uint64_t storage_request_id = next_storage_request_id();

		GetRequest get_request{
			.object_id = read.object_id,
			.chunk_index = req.chunk_index,
			.byte_count = expected_size,
			.context = {},
		};

		network_.send(replica, StorageProtocol::encode_get_request(storage_request_id, get_request));

		auto message = network_.receive_if(
			storage_request_timeout_,
			Rpc::make_message_is(
				storage_request_id,
				StorageJob::GET,
				RpcKind::Response,
				replica,
				[expected_chunk = req.chunk_index](BinaryReader &reader) {
					return reader.read<std::uint32_t>() == expected_chunk;
				}));

		if (!message) {
			continue;
		}

		try {
			auto rpc_response = Rpc::read_message<StorageJob>(std::move(*message));

			GetResponse response = StorageProtocol::decode_get_response(rpc_response);

			if (response.status != StorageStatus::Success ||
				response.chunk_index != req.chunk_index ||
				response.data.size() != expected_size) {
				continue;
			}

			chunk_data = std::move(response.data);
			chunk_received = true;
			break;
		} catch (const std::exception &) {
			continue;
		}
	}

	if (!chunk_received) {
		discard_read();
		respond_error(ClientStatus::ServerError);
		return;
	}

	read.next_chunk++;

	const bool finished = read.next_chunk == read.chunk_count;

	Frame response_frame =
		ClientProtocol::encode_read_chunk_response(
			req.context.request_id,
			ReadChunkResponse{
				.chunk_index = req.chunk_index,
				.status = ClientStatus::Success,
				.data = std::move(chunk_data),
				.context = req.context,
			});

	if (finished) discard_read();

	network_.send(req.context.sender.id, std::move(response_frame));
}

void MetadataWorker::handle(RemoveRequest req) {
	auto respond = make_responder(req.context, RemoveResponse{}, ClientProtocol::encode_remove_response);
	// Fail if file at path is currently being written
	for (const auto &client_writes : pending_writes_) {
		for (const auto &pending_entry : client_writes.second) {
			if (pending_entry.second.path == req.path) {
				respond(ClientStatus::ServerError);
				return;
			}
		}
	}

	auto metadata_result = store_.get_metadata(req.path);

	if (!metadata_result) {
		if (metadata_result.error() != MetadataStoreError::NOT_FOUND) {
			respond(get_client_status(metadata_result));
			return;
		} else {
			const auto directory_result = store_.remove_directory(req.path);

			respond(get_client_status(directory_result));
			return;
		}
	}

	const FileMetadata metadata = std::move(*metadata_result);
	const auto &replicas = metadata.replica_locations;

	const bool first_bound = !replicas[0].empty();
	const bool second_bound = !replicas[1].empty();

	// Validate replica locations are normal
	if (first_bound != second_bound || (first_bound && replicas[0] == replicas[1]) || metadata.obj_id.empty()) {
		respond(ClientStatus::ServerError);
		return;
	}

	// If file is empty/only exists on metadata node
	if (!first_bound) {
		const auto result = store_.remove_metadata(req.path);
		respond(get_client_status(result));
		return;
	}

	// Put the availability check here.
	for (const NodeId &replica : replicas) {
		const auto node = storage_nodes_.find(replica);

		if (node == storage_nodes_.end() || !node->second.available) {
			respond(ClientStatus::ServerError);
			return;
		}
	}

	const std::uint64_t storage_request_id = next_storage_request_id();

	DeleteRequest delete_request{
		.object_id = metadata.obj_id,
		.context = {},
	};

	for (const NodeId &replica : replicas) {
		network_.send(replica, StorageProtocol::encode_delete_request(storage_request_id, delete_request));
	}

	bool replicas_succeeded = true;
	for (const NodeId &replica : replicas) {

		auto message = network_.receive_if(storage_request_timeout_, Rpc::make_message_is(storage_request_id, StorageJob::DELETE, RpcKind::Response, replica));

		if (!message) {
			replicas_succeeded = false;
			continue;
		}

		try {
			auto rpc_response = Rpc::read_message<StorageJob>(std::move(*message));

			const DeleteResponse response = StorageProtocol::decode_delete_response(rpc_response);

			if (response.status != StorageStatus::Success) {
				replicas_succeeded = false;
			}
		} catch (const std::exception &) {
			replicas_succeeded = false;
		}
	}
	if (!replicas_succeeded) {
		// Keep metadata so the client can retry deletion.
		respond(ClientStatus::ServerError);
		return;
	}

	const auto result = store_.remove_metadata(req.path);
	respond(get_client_status(result));
}

void MetadataWorker::handle(ListRequest req) {
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
void MetadataWorker::handle(MakeDirRequest req) {
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

void MetadataWorker::handle(RenameRequest req) {
	auto expected = store_.rename(req.old_path, req.new_path);

	ClientStatus status = get_client_status(expected);

	Frame frame = ClientProtocol::encode_rename_response(
		req.context.request_id,
		RenameResponse{
			.status = status,
			.context = req.context,
		});

	network_.send(req.context.sender.id, std::move(frame));
}

void MetadataWorker::handle(StatRequest req) {
	auto metadata = store_.get_metadata(req.path);

	ClientStatus status = get_client_status(metadata);
	FileStat stats{};

	if (metadata) {
		status = ClientStatus::Success;

		stats = FileStat{
			.path = metadata->path.string(),
			.is_directory = false,
			.storage_nodes = metadata->replica_locations,
			.object_id = metadata->obj_id,
			.size = metadata->size,
			.created_at = metadata->created_at,
			.modified_at = metadata->modified_at,
		};
	} else if (metadata.error() == MetadataStoreError::NOT_FOUND) {
		auto directory = store_.list(req.path);

		if (directory) {
			status = ClientStatus::Success;

			stats = FileStat{
				.path = req.path,
				.is_directory = true,
				.storage_nodes = {},
				.object_id = {},
				.size = 0,
				.created_at = {},
				.modified_at = {},
			};
		} else {
			status = get_client_status(directory);
		}
	} else {
		status = get_client_status(metadata);
	}

	Frame frame = ClientProtocol::encode_stat_response(
		req.context.request_id,
		StatResponse{
			.status = status,
			.stat = stats,
			.context = req.context,
		});

	network_.send(req.context.sender.id, std::move(frame));
}