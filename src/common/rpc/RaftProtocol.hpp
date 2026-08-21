#ifndef RAFT_PROTOCOL_HPP
#define RAFT_PROTOCOL_HPP
#include "rpc/Rpc.hpp"
#include <cstdint>

enum class RaftJob : std::uint8_t {
	ELECT,
	APPEND_ENTRIES,
};

using RaftRpcMessage = RpcMessage<RaftJob>;
using RaftRpcHeader = RpcHeader<RaftJob>;

template <>
struct RpcTraits<RaftJob> {
	static constexpr MessageType message_type = MessageType::CONSENSUS;
};

#endif // RAFT_PROTOCOL_HPP