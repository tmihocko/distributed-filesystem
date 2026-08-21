#ifndef RAFTRPC_HPP
#define RAFTRPC_HPP
#include "ClientRpc.hpp"
#include "rpc/RaftProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <variant>

using RaftEvent = std::variant<RaftRpcMessage, Stop>;

class RaftRpc : public RpcService<RaftEvent, RaftRpc> {
  public:
  private:
};

#endif // RAFTRPC_HPP