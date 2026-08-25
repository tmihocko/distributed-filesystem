#include "MetadataNode.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include "rpc/StorageProtocol.hpp"
#include "rpc/RpcAdapter.hpp"
#include <print>
#include <stdexcept>

MetadataNode::MetadataNode(
	NodeConfig config,
	std::span<const Endpoint> seed_nodes)
	: config_(std::move(config)),
	  network_(config_),
	  store_("metadata/"),
	  storage_(config_, network_),
	  client_(config_, network_, storage_, store_) {
	const auto expected = store_.load_from_storage();
	if (!expected) {
		std::println("MetadataStoreError: {}", static_cast<int>(expected.error()));
		throw std::runtime_error("Failed to load metadata file");
	}
	network_.start(seed_nodes);
}

void MetadataNode::setup() {
}

void MetadataNode::start() {
	running_ = true;

	setup();

	while (running_) {
		auto message = network_.receive();
		dispatch_message(std::move(message));
	}
}

void MetadataNode::dispatch_message(Message message) {
	switch (message.header.type) {
	case MessageType::CLIENT_RPC: {
		auto rpc_message = Rpc::read_message<ClientJob>(std::move(message));

		client_.post(RpcAdapter::decode(rpc_message));

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