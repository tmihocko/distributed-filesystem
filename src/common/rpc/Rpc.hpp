#ifndef RPC_HPP
#define RPC_HPP
#include <cstdint>

enum class RpcKind : std::uint8_t {
	Response,
	Request
};

template <typename Jobs>
struct RpcHeader {
	std::uint64_t request_id;
	Jobs job;
	RpcKind kind;
};

#endif // RPC_HPP