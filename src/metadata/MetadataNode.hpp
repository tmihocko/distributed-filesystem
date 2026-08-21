/**

Owns network<Metadata>, routes all incoming messages onto Rpc dependencies

*/
#ifndef METADATANODE_HPP
#define METADATANODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "RaftRpc.hpp"
#include "ClientRpc.hpp"
#include "StorageRpc.hpp"

class MetadataNode {
  public:
	MetadataNode(Endpoint self, std::span<Endpoint> seed_nodes);

	void start();

  private:
	Network<NodeRole::METADATA> network_;

	RaftRpc raft_;
	ClientRpc client_;
	StorageRpc storage_;

	bool running_;
};

#endif // METADATANODE_HPP