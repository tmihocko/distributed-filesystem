#include "MetadataNode.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "workers/ClientService.hpp"
#include "rpc/Rpc.hpp"
#include "rpc/StorageProtocol.hpp"
#include "RpcAdapter.hpp"
#include "workers/StorageService.hpp"
#include <print>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <variant>

MetadataNode::MetadataNode(
	NodeConfig config,
	std::span<const Endpoint> seed_nodes)
	: config_(std::move(config)),
	  network_(config_),
	  store_(config_.data_directory),
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
	// idk if this needs stuff;
}

// Network producer
void MetadataNode::start() {
	setup();

	network_producer_ = std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			auto message = network_.receive();
			push_worker_job(std::move(message));
		}
	});

	// client events -> client, storage events -> storage
	worker_thread_ = std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			auto event = worker_queue_.pop();

			if (std::holds_alternative<ClientEvent>(event)) {
				auto client_event = std::get<ClientEvent>(std::move(event));

				std::visit(
					[this](auto &&event) {
						client_.handle(std::move(event));
					},
					std::move(client_event));

			} else if (std::holds_alternative<StorageEvent>(event)) {
				auto storage_event = std::get<StorageEvent>(std::move(event));

				std::visit(
					[this](auto &&event) {
						storage_.handle(std::move(event));
					},
					std::move(storage_event));
			}
		}
	});
}

void MetadataNode::stop() {
	worker_thread_.request_stop();
	network_producer_.request_stop();
}

void MetadataNode::push_worker_job(Message message) {
	switch (message.header.type) {
	case MessageType::CLIENT_RPC: {
		auto rpc_message = Rpc::read_message<ClientJob>(std::move(message));
		worker_queue_.push(RpcAdapter::decode(rpc_message));
		break;
	}
	case MessageType::STORAGE_RPC: {
		auto rpc_message = Rpc::read_message<StorageJob>(std::move(message));
		worker_queue_.push(RpcAdapter::decode(rpc_message));
		break;
	}
	default:
		// Error possibly
		break;
	}
}