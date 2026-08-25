#ifndef LOGSTORE_HPP
#define LOGSTORE_HPP
#include "network/Node.hpp"
#include <array>
#include <filesystem>
#include <unordered_map>
#include <expected>

struct FileMetadata {
	std::filesystem::path path;

	std::uint64_t size;
	std::array<NodeId, 2> replica_locations;
};

enum class MetadataStoreError {
	OPEN_FAILURE,
	MALFORMED_FILE,
	READ_FAILURE,
	WRITE_FAILURE,
	NOT_FOUND,
};

class MetadataStore {
  public:
	explicit MetadataStore(const std::filesystem::path &directory);

	// Populate RAM from persisted metadata files.
	std::expected<void, MetadataStoreError> load_from_storage();

	// RAM lookup
	[[nodiscard]]
	std::expected<FileMetadata, MetadataStoreError> get_metadata(const std::filesystem::path &filename);

	// Update RAM + persist this entry
	std::expected<void, MetadataStoreError> add_metadata(const std::filesystem::path &filename, const FileMetadata &metadata);

	std::expected<void, MetadataStoreError> remove_metadata(const std::filesystem::path &filename);

  private:
	// Add to disk
	std::expected<void, MetadataStoreError> write_metadata_file(const std::filesystem::path &filename, const FileMetadata &metadata);

	// Disk lookup
	[[nodiscard]]
	std::expected<FileMetadata, MetadataStoreError> read_metadata_file(const std::filesystem::path &path);

	std::filesystem::path metadata_path(const std::filesystem::path &filename);

	std::filesystem::path directory_;
	std::unordered_map<std::filesystem::path, FileMetadata> data_;
};
#endif // LOGSTORE_HPP
