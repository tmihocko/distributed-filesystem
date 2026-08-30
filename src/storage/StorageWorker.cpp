#include "StorageWorker.hpp"
#include "rpc/StorageProtocol.hpp"
#include "rpc/ClientProtocol.hpp"
#include "fstream"

namespace fs = std::filesystem;

void StorageWorker::handle(PutRequest req) {
	StorageStatus status;

	try {
		status = write_chunk(req);
	} catch (const std::exception &) {
		status = StorageStatus::WriteError;
	}
	Frame response = StorageProtocol::encode_put_response(
		req.context.request_id,
		PutResponse{
			.chunk_index = req.chunk_index,
			.size_available = available_bytes(),
			.status = status,
		});

	network_.send(req.context.sender.id, std::move(response));
}

void StorageWorker::handle(GetRequest req) {
	auto respond = [&](StorageStatus status, std::vector<std::byte> data = {}) {
		Frame frame = StorageProtocol::encode_get_response(
			req.context.request_id,
			GetResponse{
				.chunk_index = req.chunk_index,
				.status = status,
				.data = std::move(data),
			});

		network_.send(req.context.sender.id, std::move(frame));
	};

	if (!valid_object_id(req.object_id) || req.byte_count > CHUNK_SIZE) {
		respond(StorageStatus::InvalidRequest);
		return;
	}

	const fs::path object_path = objects_directory_ / req.object_id;

	std::ifstream input(object_path, std::ios::binary | std::ios::ate);

	if (!input) {
		respond(StorageStatus::ReadError);
		return;
	}

	const std::streampos end_position = input.tellg();

	if (end_position < 0) {
		respond(StorageStatus::ReadError);
		return;
	}

	const std::uint64_t file_size = static_cast<std::uint64_t>(end_position);
	const std::uint64_t offset = static_cast<std::uint64_t>(req.chunk_index) * static_cast<std::uint64_t>(CHUNK_SIZE);

	if (offset > file_size) {
		respond(StorageStatus::InvalidRequest);
		return;
	}

	const std::size_t bytes_to_read = static_cast<std::size_t>(std::min<std::uint64_t>(req.byte_count, file_size - offset));

	std::vector<std::byte> data(bytes_to_read);

	input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

	if (!input) {
		respond(StorageStatus::ReadError);
		return;
	}

	if (bytes_to_read != 0) {
		input.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(bytes_to_read));

		if (input.gcount() != static_cast<std::streamsize>(bytes_to_read)) {
			respond(StorageStatus::ReadError);
			return;
		}
	}

	respond(StorageStatus::Success, std::move(data));
}

void StorageWorker::handle(DeleteRequest req) {

	auto respond = [&](StorageStatus status) {
		Frame frame = StorageProtocol::encode_delete_response(
			req.context.request_id,
			DeleteResponse{ .status = status });

		network_.send(req.context.sender.id, std::move(frame));
	};

	if (!valid_object_id(req.object_id)) {
		respond(StorageStatus::InvalidRequest);
		return;
	}

	const fs::path final_path = objects_directory_ / req.object_id;

	fs::path tmp_path = final_path;
	tmp_path += ".tmp";

	std::error_code ec;

	fs::remove(final_path, ec);
	if (ec) {
		respond(StorageStatus::WriteError);
		return;
	}

	fs::remove(tmp_path, ec);
	if (ec) {
		respond(StorageStatus::WriteError);
		return;
	}

	next_chunks_.erase(req.object_id);

	respond(StorageStatus::Success);
}

StorageWorker::StorageWorker(const NodeConfig &config, Network<NodeRole::STORAGE> &network)
	: objects_directory_(std::filesystem::path(config.data_directory) / "objects"), network_(network) {
	if (config.data_directory.empty()) {
		throw std::invalid_argument("Storage data directory cannot be empty.");
	}
	std::error_code ec;
	std::filesystem::create_directories(objects_directory_, ec);

	if (ec || !std::filesystem::is_directory(objects_directory_)) {
		throw std::runtime_error("Failed to initialize storage directory.");
	}
}

StorageStatus StorageWorker::write_chunk(const PutRequest &req) {
	namespace fs = std::filesystem;

	if (!valid_object_id(req.object_id)) return StorageStatus::InvalidRequest;

	const fs::path final_path = objects_directory_ / req.object_id;

	fs::path tmp_path = final_path;
	tmp_path += ".tmp";

	if (req.chunk_index == 0) {
		next_chunks_[req.object_id] = 0;
	}

	auto next_it = next_chunks_.find(req.object_id);

	if (next_it == next_chunks_.end() || req.chunk_index != next_it->second) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::InvalidRequest;
	}

	std::error_code ec;
	const auto space = fs::space(objects_directory_, ec);

	if (ec) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::WriteError;
	}

	if (space.available < req.data.size()) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::NoSpace;
	}

	const std::ios::openmode mode = std::ios::binary | (req.chunk_index == 0 ? std::ios::trunc : std::ios::app);

	std::ofstream output(tmp_path, mode);

	if (!output) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::WriteError;
	}

	output.write(reinterpret_cast<const char *>(req.data.data()), static_cast<std::streamsize>(req.data.size()));

	output.flush();
	output.close();

	if (!output) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::WriteError;
	}
	++next_it->second;

	if (!req.final_chunk) {
		return StorageStatus::Success;
	}

	fs::rename(tmp_path, final_path, ec);

	if (ec) {
		abort_write(req.object_id, tmp_path);
		return StorageStatus::WriteError;
	}

	next_chunks_.erase(next_it);

	return StorageStatus::Success;
}

bool StorageWorker::valid_object_id(const std::string &object_id) {
	if (object_id.empty() ||
		object_id == "." ||
		object_id == "..") {
		return false;
	}

	const std::filesystem::path path{ object_id };

	return !path.is_absolute() &&
		   !path.has_parent_path() &&
		   path == path.filename();
}

void StorageWorker::abort_write(std::string_view object_id, std::filesystem::path path) {
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
}

std::uint64_t StorageWorker::available_bytes() {
	std::error_code ec;

	const auto info = std::filesystem::space(objects_directory_, ec);

	return ec ? 0 : static_cast<std::uint64_t>(info.available);
}