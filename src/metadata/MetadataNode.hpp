/**

Owns network<Metadata>, routes all incoming messages onto Rpc dependencies

*/
#ifndef METADATANODE_HPP
#define METADATANODE_HPP
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "RaftService.hpp"
#include "ClientService.hpp"
#include "StorageService.hpp"

class MetadataNode {
  public:
	MetadataNode(Endpoint self, std::span<Endpoint> seed_nodes);

	void start();

  private:
	Network<NodeRole::METADATA> network_;

	RaftService raft_;
	ClientService client_;
	StorageService storage_;

	bool running_;
};

#endif // METADATANODE_HPP