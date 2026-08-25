/**

Owns network<Metadata>, routes all incoming messages onto Rpc dependencies

*/
#ifndef METADATA_NODE_HPP
#define METADATA_NODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientService.hpp"
#include "rpc/StorageService.hpp"

class MetadataNode {
  public:
	MetadataNode(NodeConfig config, std::span<const Endpoint> seed_nodes);

	void start();

	void setup();

  private:
	void dispatch_message(Message message);

	NodeConfig config_;
	Network<NodeRole::METADATA> network_;

	MetadataStore store_;
	StorageService storage_;
	ClientService client_;

	bool running_ = false;
	bool has_setup_ = false;
};

#endif // METADATANODE_HPP