/**

Client--Metadata RPC

*/
#ifndef CLIENTPROTOCOL_HPP
#define CLIENTPROTOCOL_HPP
#include <chrono>
#include <cstdint>
#include <string>
#include "Rpc.hpp"
#include "network/Node.hpp"

enum class ClientJob : std::uint8_t {
	CREATE_FILE,
	READ_FILE,
	WRITE_FILE,
	REMOVE,
	LIST,
	MKDIR,
	RENAME,
	STAT,

	// Internal usage
	WRITE_CHUNK,
};

enum class ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
	BadResponse,
	BadInput,
	NotImplemented,
	Timeout,
	StorageFull,
	ReadError,
};

struct FileInfo {
	std::string path;
	bool is_directory;
};

struct FileStat {
	bool is_directory; // false = file, true = directory

	std::array<NodeId, 2> storage_nodes; // machine storing the object
	std::string object_id;				 // stable identity in your DFS

	std::uint64_t size; // logical size in bytes

	std::chrono::system_clock::time_point created_at;
	std::chrono::system_clock::time_point modified_at;

	void print();
};

using ClientRpcMessage = RpcMessage<ClientJob>;
using ClientRpcHeader = RpcHeader<ClientJob>;

template <>
struct RpcTraits<ClientJob> {
	static constexpr MessageType message_type = MessageType::CLIENT_RPC;
};

enum class ClientStatus : std::uint8_t {
	Success = 0,
	ServerError = 1,
	InputError = 2,
	AlreadyExists = 3,
};

struct RequestContext {
	std::uint64_t request_id;
	NodeIdentity sender;

	template <typename T>
	static RequestContext from(const RpcMessage<T> &message) {
		return RequestContext{
			.request_id = message.rpc_header.request_id,
			.sender = message.sender,
		};
	}
};

// Size of chunk for data sent on big requests like write and read
constexpr std::uint32_t CHUNK_SIZE = 512 * 1024;

struct CreateFileRequest {
	std::string path;
	RequestContext context;
};
struct CreateFileResponse {
	ClientStatus status;
	RequestContext context;
};

//

struct WriteFileRequest {
	std::uint64_t file_size;
	std::uint32_t chunk_count; // 18 petabyte max
	std::string path;
	RequestContext context;
};

struct WriteFileResponse {
	ClientStatus status;
	RequestContext context;
};

struct WriteChunkRequest {
	std::uint32_t chunk_index;
	std::vector<std::byte> data;
	RequestContext context;
};

struct WriteChunkResponse {
	std::uint32_t chunk_index;
	ClientStatus status;
	RequestContext context;
};

//

struct RemoveRequest {
	std::string path;
	RequestContext context;
};

struct RemoveResponse {
	ClientStatus status;
};

//

struct ListRequest {
	std::string path;
	RequestContext context;
};

// Rpc messages have uint32 size before
struct ListResponse {
	ClientStatus status;
	std::vector<FileInfo> info_vec;
	RequestContext context;
};

//

struct MakeDirRequest {
	std::string path;
	RequestContext context;
};
struct MakeDirResponse {
	ClientStatus status;
	RequestContext context;
};

//

struct RenameRequest {
	std::string old_path;
	std::string new_path;
	RequestContext context;
};
struct RenameResponse {
	ClientStatus status;
	RequestContext context;
};

//

struct StatRequest {
	std::string path;
	RequestContext context;
};

struct StatResponse {
	ClientStatus status;
	FileStat stat;
	RequestContext context;
};

namespace ClientProtocol {

Frame encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request);
CreateFileRequest decode_create_file_request(const ClientRpcMessage &message);

Frame encode_create_file_response(std::uint64_t request_id, const CreateFileResponse &response);
CreateFileResponse decode_create_file_response(const ClientRpcMessage &message);

//

Frame encode_write_file_request(std::uint64_t request_id, const WriteFileRequest &request);
WriteFileRequest decode_write_file_request(const ClientRpcMessage &message);

Frame encode_write_chunk_request(std::uint64_t request_id, const WriteChunkRequest &request);
WriteChunkRequest decode_write_chunk_request(const ClientRpcMessage &message);

Frame encode_write_chunk_response(std::uint64_t request_id, const WriteChunkResponse &request);
WriteChunkResponse decode_write_chunk_response(const ClientRpcMessage &message);

Frame encode_write_file_response(std::uint64_t request_id, const WriteFileResponse &response);
WriteFileResponse decode_write_file_response(const ClientRpcMessage &message);

//

Frame encode_remove_request(std::uint64_t request_id, const RemoveRequest &request);
RemoveRequest decode_remove_request(const ClientRpcMessage &message);

Frame encode_remove_response(std::uint64_t request_id, const RemoveResponse &response);
RemoveResponse decode_remove_response(const ClientRpcMessage &message);

//

Frame encode_list_request(std::uint64_t request_id, const ListRequest &request);
ListRequest decode_list_request(const ClientRpcMessage &message);

Frame encode_list_response(std::uint64_t request_id, const ListResponse &response);
ListResponse decode_list_response(const ClientRpcMessage &message);

//

Frame encode_mkdir_request(std::uint64_t request_id, const MakeDirRequest &request);
MakeDirRequest decode_mkdir_request(const ClientRpcMessage &message);

Frame encode_mdkir_response(std::uint64_t request_id, const MakeDirResponse &response);
MakeDirResponse decode_mkdir_response(const ClientRpcMessage &message);

//

Frame encode_rename_request(std::uint64_t request_id, const RenameRequest &request);
RenameRequest decode_rename_request(const ClientRpcMessage &message);

Frame encode_rename_response(std::uint64_t request_id, const RenameResponse &response);
RenameResponse decode_rename_response(const ClientRpcMessage &message);

//

Frame encode_stat_request(std::uint64_t request_id, const StatRequest &request);
StatRequest decode_stat_request(const ClientRpcMessage &message);

Frame encode_stat_response(std::uint64_t request_id, const StatResponse &response);
StatResponse decode_stat_response(const ClientRpcMessage &message);

} // namespace ClientProtocol

#endif // CLIENTPROTOCOL_HPP