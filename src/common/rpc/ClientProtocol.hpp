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
};

using ClientRpcHeader = RpcHeader<ClientJob>;

#endif // CLIENTPROTOCOL_HPP