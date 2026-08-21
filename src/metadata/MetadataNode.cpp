#include "MetadataNode.hpp"
#include "network/Message.hpp"
#include "rpc/Rpc.hpp"
#include "rpc/RaftProtocol.hpp"
#include "rpc/StorageProtocol.hpp"

MetadataNode::MetadataNode(Endpoint self, std::span<Endpoint> seed_nodes) : network_(self) {
	network_.start(seed_nodes);
}

void MetadataNode::start() {
	running_ = true;

	while (running_) {
		auto message = network_.receive();

		switch (message.header.type) {
		case MessageType::CLIENT_RPC:
			// Change will isolate packet reads/writes from handle(ClientEvent)
			// This decode should return a ClientEvent
			// client_.post(ClientProtocol::decode(std::move(message)));

			client_.post(Rpc::read_message<ClientJob>(std::move(message)));
			break;
		case MessageType::CONSENSUS:
			raft_.post(Rpc::read_message<RaftJob>(std::move(message)));
			break;
		case MessageType::STORAGE_RPC:
			storage_.post(Rpc::read_message<StorageJob>(std::move(message)));
			break;
		default:
			// Error possibly
			break;
		}
	}
}