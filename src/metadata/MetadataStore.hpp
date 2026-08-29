#ifndef LOGSTORE_HPP
#define LOGSTORE_HPP
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include <array>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <expected>

static constexpr std::uint8_t METADATA_MAGIC = 0x69;
static constexpr std::uint8_t METADATA_VERSION = 0x01;

struct StorageNodeInfo {
	std::chrono::steady_clock::time_point last_seen;
	std::uint64_t bytes_left;
	bool available = true;
}; // Not actually for the store, just here to solve cyclic dependency

struct FileMetadata {
	std::filesystem::path path;

	std::uint64_t size;
	std::string obj_id; // Filename in storage node
	std::array<NodeId, 2> replica_locations;

	std::chrono::system_clock::time_point created_at{};
	std::chrono::system_clock::time_point modified_at{};
};

enum class MetadataStoreError {
	OPEN_FAILURE,
	MALFORMED_FILE,
	READ_FAILURE,
	WRITE_FAILURE,
	NOT_FOUND,
	WRONG_VERSION,
	INVALID_PATH,
	ALREADY_EXISTS,
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
	std::expected<void, MetadataStoreError> add_metadata(const std::filesystem::path &filename, FileMetadata metadata);

	std::expected<void, MetadataStoreError> update_metadata(const std::filesystem::path &filename, FileMetadata metadata);

	std::expected<void, MetadataStoreError> remove_metadata(const std::filesystem::path &filename);

	std::expected<void, MetadataStoreError> make_directory(const std::filesystem::path &path);

	std::expected<void, MetadataStoreError> remove_directory(const std::filesystem::path &path);

	[[nodiscard]]
	std::expected<std::vector<FileInfo>, MetadataStoreError> list(const std::filesystem::path &path);

	std::expected<void, MetadataStoreError> rename(const std::filesystem::path &old_path, const std::filesystem::path &new_path);

  private:
	// Add to disk
	std::expected<void, MetadataStoreError> write_metadata_file(const std::filesystem::path &filename, const FileMetadata &metadata);

	// Disk lookup
	[[nodiscard]]
	std::expected<FileMetadata, MetadataStoreError> read_metadata_file(const std::filesystem::path &path);

	bool valid_filename(const std::filesystem::path &filename);

	std::filesystem::path directory_;
	std::unordered_map<std::filesystem::path, FileMetadata> data_;
};
#endif // LOGSTORE_HPP
