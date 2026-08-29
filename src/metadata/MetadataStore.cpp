#include "MetadataStore.hpp"
#include "Serializer.hpp"
#include "rpc/ClientProtocol.hpp"
#include <expected>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

MetadataStore::MetadataStore(const fs::path &directory) : directory_(directory) {
	std::error_code ec;

	fs::create_directories(directory_, ec);

	if (ec || !fs::is_directory(directory_)) {
		throw std::runtime_error("Failed to initialize metadata directory");
	}
}

std::expected<void, MetadataStoreError> MetadataStore::write_metadata_file(const fs::path &filename, const FileMetadata &metadata) {
	BinaryWriter writer;

	writer.write(METADATA_MAGIC, METADATA_VERSION, metadata.size, metadata.obj_id, metadata.replica_locations[0], metadata.replica_locations[1]);

	const std::streamsize length = writer.length();
	auto data = writer.move_data();

	const fs::path final_path = directory_ / filename; // fix rn it does like metadata/obj/.tmp

	std::error_code ec;
	fs::create_directories(final_path.parent_path(), ec);

	if (ec == std::errc::file_exists || ec == std::errc::not_a_directory) {
		return std::unexpected(MetadataStoreError::INVALID_PATH);
	}

	if (ec) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	fs::path temporary_path = final_path;
	temporary_path += ".tmp";

	std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);

	if (!file) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	if (!file.write(reinterpret_cast<const char *>(data.data()), length)) {
		return std::unexpected(MetadataStoreError::WRITE_FAILURE);
	}

	file.flush();
	if (!file) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	file.close();
	if (!file) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	// Atomic on posix (overwrite), can error on some windows versions
	fs::rename(temporary_path, final_path, ec);

	if (ec) {
		std::error_code cleanup_ec;
		fs::remove(temporary_path, cleanup_ec);
		return std::unexpected(MetadataStoreError::WRITE_FAILURE);
	}

	return {};
}

std::expected<FileMetadata, MetadataStoreError> MetadataStore::read_metadata_file(const fs::path &filename) {
	const fs::path physical_path = directory_ / filename;

	std::ifstream file(physical_path, std::ios::binary | std::ios::ate);
	if (!file) return std::unexpected(MetadataStoreError::READ_FAILURE);

	const auto size = file.tellg();
	if (size < 0) return std::unexpected(MetadataStoreError::READ_FAILURE);

	std::vector<std::byte> bytes(static_cast<std::size_t>(size));

	file.seekg(0, std::ios::beg);

	if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
		return std::unexpected(MetadataStoreError::READ_FAILURE);
	}

	try {
		BinaryReader reader{ bytes };

		const auto &[magic, version, file_size, obj_id, rep1, rep2] =
			reader.read<std::uint8_t, std::uint8_t, std::uint64_t, std::string, NodeId, NodeId>();

		if (magic != METADATA_MAGIC) return std::unexpected(MetadataStoreError::MALFORMED_FILE);
		if (version != METADATA_VERSION) return std::unexpected(MetadataStoreError::WRONG_VERSION);
		if (!reader.at_end()) return std::unexpected(MetadataStoreError::MALFORMED_FILE);

		return FileMetadata{
			.path = filename,
			.size = file_size,
			.obj_id = obj_id,
			.replica_locations = { rep1, rep2 },
		};
	} catch (const std::out_of_range &) {
		return std::unexpected(MetadataStoreError::MALFORMED_FILE);
	} catch (const std::runtime_error &) {
		return std::unexpected(MetadataStoreError::MALFORMED_FILE);
	}
}

// Only called during startup
std::expected<void, MetadataStoreError> MetadataStore::load_from_storage() {
	data_.clear();

	std::error_code ec;
	fs::recursive_directory_iterator it(directory_, ec);
	const fs::recursive_directory_iterator end;

	if (ec) return std::unexpected(MetadataStoreError::READ_FAILURE);

	while (it != end) {
		const auto &entry = *it;

		const bool is_regular = entry.is_regular_file(ec);
		if (ec) return std::unexpected(MetadataStoreError::READ_FAILURE);

		if (is_regular) {
			const fs::path filename = entry.path().lexically_relative(directory_);

			if (filename.extension() != ".tmp") {
				auto metadata = read_metadata_file(filename);
				if (!metadata) return std::unexpected(metadata.error());

				const auto [_, inserted] = data_.emplace(filename, std::move(*metadata));
				if (!inserted) return std::unexpected(MetadataStoreError::MALFORMED_FILE); // Overlapping names, idk if this is possible
			}
		}

		it.increment(ec);
		if (ec) return std::unexpected(MetadataStoreError::READ_FAILURE);
	}

	return {};
}

std::expected<FileMetadata, MetadataStoreError> MetadataStore::get_metadata(const fs::path &filename) {
	if (!valid_filename(filename)) return std::unexpected(MetadataStoreError::INVALID_PATH);

	auto it = data_.find(filename);

	if (it == data_.end()) return std::unexpected(MetadataStoreError::NOT_FOUND);

	return it->second;
}

std::expected<void, MetadataStoreError> MetadataStore::add_metadata(
	const fs::path &filename,
	FileMetadata metadata) {
	if (!valid_filename(filename)) return std::unexpected(MetadataStoreError::INVALID_PATH);

	std::error_code ec;
	const bool already_exists = fs::exists(directory_ / filename, ec);

	if (ec) return std::unexpected(MetadataStoreError::WRITE_FAILURE);
	if (already_exists) return std::unexpected(MetadataStoreError::ALREADY_EXISTS);

	metadata.path = filename; // metadata is a copy btw
	auto result = write_metadata_file(filename, metadata);

	if (!result) return result;

	data_.emplace(filename, std::move(metadata));

	return {};
}

std::expected<void, MetadataStoreError> MetadataStore::update_metadata(const fs::path &filename, FileMetadata metadata) {
	if (!valid_filename(filename)) return std::unexpected(MetadataStoreError::INVALID_PATH);

	if (!data_.contains(filename)) {
		return add_metadata(filename, std::move(metadata));
	}

	metadata.path = filename;

	auto result = write_metadata_file(filename, metadata);

	if (!result) return result;

	data_.insert_or_assign(filename, std::move(metadata));

	return {};
}

std::expected<void, MetadataStoreError> MetadataStore::remove_metadata(const fs::path &filename) {
	if (!valid_filename(filename)) return std::unexpected(MetadataStoreError::INVALID_PATH);

	const auto path = directory_ / filename;

	std::error_code ec;

	if (!fs::remove(path, ec)) {
		if (ec) {
			return std::unexpected(MetadataStoreError::WRITE_FAILURE);
		} else {
			return std::unexpected(MetadataStoreError::NOT_FOUND);
		}
	}

	data_.erase(filename);

	return {};
}

bool MetadataStore::valid_filename(const fs::path &filename) {
	if (filename.empty() ||
		filename.is_absolute() ||
		filename.filename().empty() ||
		filename.extension() == ".tmp" ||
		filename != filename.lexically_normal()) {
		return false;
	}

	for (const auto &component : filename) {
		if (component == "." || component == "..") return false;
	}

	return true;
}

std::expected<void, MetadataStoreError> MetadataStore::make_directory(const fs::path &path) {
	if (!valid_filename(path)) return std::unexpected(MetadataStoreError::INVALID_PATH);

	const auto final_path = directory_ / path.relative_path();

	std::error_code ec;
	const bool created = fs::create_directory(final_path, ec);

	if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory) {
		return std::unexpected(MetadataStoreError::INVALID_PATH);
	} else if (ec) {
		return std::unexpected(MetadataStoreError::WRITE_FAILURE);
	}

	if (!created) return std::unexpected(MetadataStoreError::ALREADY_EXISTS);

	return {};
}

std::expected<std::vector<FileInfo>, MetadataStoreError> MetadataStore::list(const fs::path &path) {
	std::vector<FileInfo> list;
	std::error_code ec;

	const auto final_path = directory_ / path.relative_path();

	if (!fs::exists(final_path, ec) || !fs::is_directory(final_path, ec)) return std::unexpected(MetadataStoreError::INVALID_PATH);
	if (ec) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	for (const auto &entry : fs::directory_iterator(final_path, ec)) {
		if (ec) return std::unexpected(MetadataStoreError::WRITE_FAILURE);
		list.emplace_back(entry.path().filename(), entry.is_directory());
	}

	return list;
}
