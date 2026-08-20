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

using StorageRpcHeader = RpcHeader<StorageJob>;

#endif // STORAGEPROTOCOL_HPP