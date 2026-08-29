#ifndef STORAGEWORKER_HPP
#define STORAGEWORKER_HPP
#include <filesystem>
#include <system_error>
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"
#include "fstream"

class StorageWorker {
  public:
	void handle(PutRequest req) {
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

	void handle(DeleteRequest req) {
		namespace fs = std::filesystem;

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

	StorageWorker(const NodeConfig &config, Network<NodeRole::STORAGE> &network)
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

  private:
	bool valid_object_id(const std::string &object_id) {
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
	StorageStatus write_chunk(const PutRequest &req) {
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

	void abort_write(std::string_view object_id, std::filesystem::path path) {
		std::error_code ignored;
		std::filesystem::remove(path, ignored);
	}

	std::uint64_t available_bytes() {
		std::error_code ec;

		const auto info = std::filesystem::space(objects_directory_, ec);

		return ec ? 0 : static_cast<std::uint64_t>(info.available);
	}

	std::filesystem::path objects_directory_;
	NodeConfig config_;
	Network<NodeRole::STORAGE> &network_;

	std::unordered_map<std::string, std::uint32_t> next_chunks_;

}; // Does worker jobs

#endif // STORAGEWORKER_HPP