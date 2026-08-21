#ifndef STORAGERPC_HPP
#define STORAGERPC_HPP
#include <variant>
#include "ClientRpc.hpp"
#include "rpc/StorageProtocol.hpp"

using StorageEvent = std::variant<StorageRpcMessage, Stop>;

class StorageRpc : public RpcService<StorageEvent, StorageRpc> {
  public:
  private:
};

#endif // STORAGERPC_HPP