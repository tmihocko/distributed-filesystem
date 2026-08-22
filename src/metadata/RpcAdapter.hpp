#ifndef DECODEMESSAGE_HPP
#define DECODEMESSAGE_HPP
#include "ClientService.hpp"
#include "RaftService.hpp"
#include "StorageService.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/RaftProtocol.hpp"
#include "rpc/StorageProtocol.hpp"

namespace RpcAdapter {

ClientEvent decode(ClientRpcMessage message);

StorageEvent decode(StorageRpcMessage message);

RaftEvent decode(RaftRpcMessage message);

} // namespace RpcAdapter

#endif // DECODEMESSAGE_HPP