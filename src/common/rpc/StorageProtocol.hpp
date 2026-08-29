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

namespace StorageProtocol {

Frame encode_put_request(std::uint64_t request_id, const PutRequest &request);

PutRequest decode_put_request(const StorageRpcMessage &message);

Frame encode_put_response(std::uint64_t request_id, const PutResponse &response);

PutResponse decode_put_response(const StorageRpcMessage &message);

} // namespace StorageProtocol

#endif // STORAGEPROTOCOL_HPP