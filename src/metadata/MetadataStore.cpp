#include "MetadataStore.hpp"
#include "Serializer.hpp"
#include "Serializer.hpp"
#include "Serializer.hpp"
#include <expected>
#include <fstream>

namespace fs = std::filesystem;

MetadataStore::MetadataStore(const std::filesystem::path &directory) : directory_(directory) {
	std::error_code ec;

	std::filesystem::create_directories(directory_, ec);

	if (ec || !std::filesystem::is_directory(directory_)) {
		throw std::runtime_error("Failed to initialize metadata directory");
	}
}

std::expected<void, MetadataStoreError> MetadataStore::write_metadata_file(const std::filesystem::path &path, const FileMetadata &metadata) {
	BinaryWriter writer;

	writer.write(metadata.size, metadata.replica_locations[0], metadata.replica_locations[1]);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);

	if (!file) return std::unexpected(MetadataStoreError::WRITE_FAILURE);

	const std::streamsize length = writer.length();
	auto data = writer.move_data();

	if (!file.write(reinterpret_cast<const char *>(data.data()), length)) {
		return std::unexpected(MetadataStoreError::WRITE_FAILURE);
	}

	return {};
}

std::expected<FileMetadata, MetadataStoreError> MetadataStore::read_metadata_file(const std::filesystem::path &path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) return std::unexpected(MetadataStoreError::READ_FAILURE);

	const auto size = file.tellg();
	if (size < 0) return std::unexpected(MetadataStoreError::READ_FAILURE);

	std::vector<std::byte> bytes(static_cast<std::size_t>(size));

	file.seekg(0, std::ios::beg);

	if (!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
		return std::unexpected(MetadataStoreError::READ_FAILURE);
	}

	BinaryReader reader{ bytes };

	const auto &[file_size, rep1, rep2] = reader.read<std::uint64_t, NodeId, NodeId>();
	if (!reader.at_end()) return std::unexpected(MetadataStoreError::MALFORMED_FILE);

	return FileMetadata{
		.path = path,
		.size = file_size,
		.replica_locations = { rep1, rep2 },
	};
}

std::expected<void, MetadataStoreError> MetadataStore::load_from_storage() {
	data_.clear();

	std::error_code ec;

	for (const auto &entry : fs::directory_iterator(directory_, ec)) {
		if (ec) return std::unexpected(MetadataStoreError::READ_FAILURE);
		if (!entry.is_regular_file()) continue;

		auto metadata = read_metadata_file(entry.path());

		if (!metadata) return std::unexpected(metadata.error());

		data_.emplace(
			entry.path().filename(),
			std::move(*metadata));
	}

	return {};
}

std::expected<FileMetadata, MetadataStoreError> MetadataStore::get_metadata(const std::filesystem::path &filename) {
	auto it = data_.find(filename);

	if (it == data_.end()) return std::unexpected(MetadataStoreError::NOT_FOUND);

	return it->second;
}

std::expected<void, MetadataStoreError> MetadataStore::add_metadata(
	const std::filesystem::path &filename,
	const FileMetadata &metadata) {
	auto result = write_metadata_file(filename, metadata);

	if (!result) return result;

	data_[filename] = metadata;

	return {};
}

std::expected<void, MetadataStoreError> MetadataStore::remove_metadata(const std::filesystem::path &filename) {
	const auto path = metadata_path(filename);

	std::error_code ec;

	if (!std::filesystem::remove(path, ec)) {
		if (ec) {
			return std::unexpected(MetadataStoreError::WRITE_FAILURE);
		} else {
			return std::unexpected(MetadataStoreError::NOT_FOUND);
		}
	}

	data_.erase(filename);

	return {};
}
