#include "MetadataNode.hpp"
#include "network/Message.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include "rpc/RaftProtocol.hpp"
#include "RpcAdapter.hpp"
#include "rpc/StorageProtocol.hpp"

MetadataNode::MetadataNode(Endpoint self, std::span<Endpoint> seed_nodes) : network_(self), client_(network_) {
	network_.start(seed_nodes);
}

void MetadataNode::start() {
	running_ = true;

	while (running_) {
		auto message = network_.receive();

		switch (message.header.type) {
		case MessageType::CLIENT_RPC: {
			auto rpc_message = Rpc::read_message<ClientJob>(std::move(message));
			if (raft_.is_leader()) {
				client_.post(RpcAdapter::decode(std::move(rpc_message)));
			} else {
				client_.post(LeaderHintRequest{ RequestContext{
					.request_id = rpc_message.rpc_header.request_id,
					.sender = std::move(rpc_message.sender) } });
			}
			break;
		}
		case MessageType::CONSENSUS: {
			auto rpc_message = Rpc::read_message<RaftJob>(std::move(message));
			raft_.post(RpcAdapter::decode(rpc_message));
			break;
		}
		case MessageType::STORAGE_RPC: {
			auto rpc_message = Rpc::read_message<StorageJob>(std::move(message));
			storage_.post(RpcAdapter::decode(rpc_message));
			break;
		}
		default:
			// Error possibly
			break;
		}
	}
}