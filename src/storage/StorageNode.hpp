#ifndef STORAGENODE_HPP
#define STORAGENODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"
#include "util/BlockingQueue.hpp"
#include "StorageWorker.hpp"
#include "StorageHeart.hpp"
#include <span>
#include <stop_token>
#include <thread>

using StorageEvent = std::variant<StorageRpcMessage>;

class StorageNode {
  public:
	StorageNode(NodeConfig config, std::span<const Endpoint> seed_nodes)
		: config_(config), network_(config), heart_(network_, metadata_node_id(seed_nodes)) {
		network_.start(seed_nodes);
	}

	void start() {
		network_producer_ = std::jthread([this](std::stop_token stop) {
			while (!stop.stop_requested()) {
				auto message = network_.receive();
			}
		});

		worker_thread_ = std::jthread([this](std::stop_token stop) {
			while (!stop.stop_requested()) {
				auto job = job_queue_.pop();
				std::visit([this](auto &&event) {
					worker_.handle(std::move(event));
				}, std::move(job));
			} });

		heartbeat_thread_ = std::jthread([this](std::stop_token stop) {
			while (!stop.stop_requested()) {
				heart_.beat();
				std::this_thread::sleep_for(heartrate_);
			}
		});
	}

	void stop() {
		network_producer_.request_stop();
		worker_thread_.request_stop();
		heartbeat_thread_.request_stop();
	}

  private:
	NodeConfig config_;
	Network<NodeRole::STORAGE> network_;

	BlockingQueue<StorageEvent> job_queue_;

	StorageWorker worker_;
	StorageHeart heart_;

	std::chrono::nanoseconds heartrate_{ 3000000000 };

	std::jthread network_producer_;
	std::jthread worker_thread_;	// consumer
	std::jthread heartbeat_thread_; // sends heartbeats to metadata node

	static NodeId metadata_node_id(std::span<const Endpoint> seed_nodes) {
		for (const auto &node : seed_nodes) {
			if (node.role == NodeRole::METADATA) {
				return node.node_id;
			}
		}

		throw std::invalid_argument("No metadata node found.");
	}
};

#endif // STORAGENODE_HPP