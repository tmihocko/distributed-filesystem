/**

Owns network<Metadata>, routes all incoming messages onto Rpc dependencies

*/
#ifndef METADATA_NODE_HPP
#define METADATA_NODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "workers/ClientService.hpp"
#include "workers/StorageService.hpp"
#include "util/BlockingQueue.hpp"

using Event = std::variant<ClientEvent, StorageEvent>;

class MetadataNode {
  public:
	MetadataNode(NodeConfig config, std::span<const Endpoint> seed_nodes);

	void start();

	void stop();

  private:
	void push_worker_job(Message message);

	void setup();

	NodeConfig config_;
	Network<NodeRole::METADATA> network_;

	MetadataStore store_;
	StorageService storage_;
	ClientService client_;

	std::chrono::duration<int, std::milli> timeout_; // = something

	BlockingQueue<Event> worker_queue_;
	// BlockingQueue<int> heartbeat_queue_; ??? maybe this is a vector, one heartbeat thread for each storage node

	std::jthread network_producer_;
	std::jthread worker_thread_; // consumer

	bool has_setup_ = false;
};

#endif // METADATANODE_HPP