#include "RpcAdapter.hpp"
#include "workers/ClientService.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <cassert>
#include <stdexcept>

ClientEvent RpcAdapter::decode(ClientRpcMessage message) {
	assert(message.rpc_header.kind != RpcKind::Response); // Client cannot/should not respond to metadata

	switch (message.rpc_header.job) {

	case ClientJob::CREATE_FILE:
		return ClientProtocol::decode_create_file_request(message);

	case ClientJob::NOT_LEADER:
		throw std::runtime_error("Client cannot send NOT_LEADER job.");

	default:
		throw std::runtime_error("Job type not handled.");
	}
}

StorageEvent RpcAdapter::decode(StorageRpcMessage message) {
	throw std::runtime_error("Not implemented");
}
