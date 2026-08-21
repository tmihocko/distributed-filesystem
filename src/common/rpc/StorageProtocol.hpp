#ifndef STORAGEPROTOCOL_HPP
#define STORAGEPROTOCOL_HPP
#include <cstdint>
#include "Rpc.hpp"

enum class StorageJob : std::uint8_t {
	PUT_CHUNK,
	GET_CHUNK,
	DELETE_CHUNK,
	HAS_CHUNK,
};

using StorageRpcMessage = RpcMessage<StorageJob>;
using StorageRpcHeader = RpcHeader<StorageJob>;

template <>
struct RpcTraits<StorageJob> {
	static constexpr MessageType message_type =
		MessageType::STORAGE_RPC;
};

#endif // STORAGEPROTOCOL_HPP