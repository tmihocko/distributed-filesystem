/**

Client--Metadata RPC

*/
#ifndef CLIENTPROTOCOL_HPP
#define CLIENTPROTOCOL_HPP
#include <cstdint>
#include "Rpc.hpp"

enum class ClientJob : std::uint8_t {
	CREATE_FILE,
	READ_FILE,
	WRITE_FILE,
	REMOVE,
	LIST,
	MKDIR,
	RENAME,
	STAT,

	LEADER_HINT = 0xFF, //
};

using ClientRpcMessage = RpcMessage<ClientJob>;
using ClientRpcHeader = RpcHeader<ClientJob>;

template <>
struct RpcTraits<ClientJob> {
	static constexpr MessageType message_type = MessageType::CLIENT_RPC;
};

#endif // CLIENTPROTOCOL_HPP