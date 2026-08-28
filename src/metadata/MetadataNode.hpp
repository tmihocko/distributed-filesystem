/**

Owns network<Metadata>, routes all incoming messages onto Rpc dependencies,

Basically the leader object of everything in this process

*/
#ifndef METADATA_NODE_HPP
#define METADATA_NODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/StorageProtocol.hpp"
#include "MetadataWorker.hpp"
#include "util/BlockingQueue.hpp"
#include <chrono>
#include <unordered_map>

struct StorageNodeInfo {
	std::chrono::steady_clock::time_point last_seen;
	std::uint64_t bytes_left;
	bool available = true;
};

using Event = std::variant<ClientEvent, StorageHeartbeat>;

class MetadataNode {
  public:
	MetadataNode(NodeConfig config, std::span<const Endpoint> seed_nodes);

	void start();

	void stop();

  private:
	void handle_heartbeat(StorageHeartbeat heartbeat);
	void check_heartbeat_timeouts();

	void push_worker_job(Message message);

	void setup();

	NodeConfig config_;
	Network<NodeRole::METADATA> network_;

	MetadataStore store_;
	MetadataWorker client_;

	std::chrono::seconds network_receive_timeout_{ 1 };
	std::chrono::seconds heartbeat_check_interval_{ 1 };
	std::chrono::seconds heartbeat_timeout_{ 10 };

	BlockingQueue<Event> job_queue_;
	// BlockingQueue<int> heartbeat_queue_; ??? maybe this is a vector, one heartbeat thread for each storage node

	std::jthread network_producer_;
	std::jthread worker_thread_; // consumer

	std::unordered_map<NodeId, StorageNodeInfo> storage_nodes;

	bool has_setup_ = false;
};

#endif // METADATANODE_HPP