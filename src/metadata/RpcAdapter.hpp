/**
Converts rpc message into Protocol Events
*/
#ifndef RPC_ADAPTER_HPP
#define RPC_ADAPTER_HPP
#include "workers/ClientService.hpp"
#include "workers/StorageService.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/StorageProtocol.hpp"

namespace RpcAdapter {

ClientEvent decode(ClientRpcMessage message);

StorageEvent decode(StorageRpcMessage message);

} // namespace RpcAdapter

#endif // RPC_ADAPTER_HPP