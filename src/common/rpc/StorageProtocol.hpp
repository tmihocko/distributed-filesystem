#ifndef STORAGEPROTOCOL_HPP
#define STORAGEPROTOCOL_HPP
#include <cstdint>
#include "Rpc.hpp"

enum class StorageJob : std::uint8_t {
	PUT,
	GET,
	DELETE,

	// Exists? Size? checksum
	STAT,

	// file, generation, destination
	// Send bytes to storage, that way metadata bandwidth doesnt bottleneck
	COPY,

	// What do i have
	LIST,

	// To metadata
	HEARTBEAT,
};

using StorageRpcMessage = RpcMessage<StorageJob>;
using StorageRpcHeader = RpcHeader<StorageJob>;

template <>
struct RpcTraits<StorageJob> {
	static constexpr MessageType message_type =
		MessageType::STORAGE_RPC;
};

#endif // STORAGEPROTOCOL_HPP