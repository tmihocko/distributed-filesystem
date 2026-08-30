#ifndef STORAGEPROTOCOL_HPP
#define STORAGEPROTOCOL_HPP
#include <cstdint>
#include "Rpc.hpp"
#include "rpc/ClientProtocol.hpp"

struct StorageHeartbeat {
	NodeIdentity sender;
};

enum class StorageJob : std::uint8_t {
	PUT,
	GET,
	DELETE,
};

using StorageRpcMessage = RpcMessage<StorageJob>;
using StorageRpcHeader = RpcHeader<StorageJob>;

template <>
struct RpcTraits<StorageJob> {
	static constexpr MessageType message_type =
		MessageType::STORAGE_RPC;
};

enum class StorageStatus : std::uint8_t {
	Success,
	NoSpace,
	WriteError,
	ReadError,
	InvalidRequest,
};

struct PutRequest {
	std::string object_id;
	std::uint32_t chunk_index;
	bool final_chunk;
	std::vector<std::byte> data;
	RequestContext context;
};

struct PutResponse {
	std::uint32_t chunk_index;
	std::uint64_t size_available; // total storage size available in this machine
	StorageStatus status;
};

//

struct GetRequest {
	std::string object_id;
	std::uint32_t chunk_index;
	std::uint32_t byte_count;
	RequestContext context;
};

struct GetResponse {
	std::uint32_t chunk_index;
	StorageStatus status;
	std::vector<std::byte> data;
};

//

struct DeleteRequest {
	std::string object_id;
	RequestContext context;
};

struct DeleteResponse {
	StorageStatus status;
};

namespace StorageProtocol {

Frame encode_put_request(std::uint64_t request_id, const PutRequest &request);
PutRequest decode_put_request(const StorageRpcMessage &message);

Frame encode_put_response(std::uint64_t request_id, const PutResponse &response);
PutResponse decode_put_response(const StorageRpcMessage &message);

//

Frame encode_get_request(std::uint64_t request_id, const GetRequest &request);
GetRequest decode_get_request(const StorageRpcMessage &message);

Frame encode_get_response(std::uint64_t request_id, const GetResponse &response);
GetResponse decode_get_response(const StorageRpcMessage &message);

//

Frame encode_delete_request(std::uint64_t request_id, const DeleteRequest &request);
DeleteRequest decode_delete_request(const StorageRpcMessage &message);

Frame encode_delete_response(std::uint64_t request_id, const DeleteResponse &response);
DeleteResponse decode_delete_response(const StorageRpcMessage &message);

} // namespace StorageProtocol

#endif // STORAGEPROTOCOL_HPP