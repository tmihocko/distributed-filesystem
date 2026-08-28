#ifndef STORAGEPROTOCOL_HPP
#define STORAGEPROTOCOL_HPP
#include <cstdint>
#include "Rpc.hpp"

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

#endif // STORAGEPROTOCOL_HPP