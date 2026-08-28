#ifndef STORAGEHEART_HPP
#define STORAGEHEART_HPP

#include "network/Network.hpp"

// Just sends heartbeat, in its own class cuz heartbeat might become bidirectional if needed
class StorageHeart {
  public:
	StorageHeart(Network<NodeRole::STORAGE> &network, NodeId metadata) : network_(network), metadata_node_(metadata) {}

	void beat() {
		Frame heartbeat{
			.header = MessageHeader{
				.magic = HEADER_MAGIC,
				.length = 0,
				.type =
					MessageType::STORAGE_HEARTBEAT,
			},
			.buffer = {},
		};

		network_.send(metadata_node_, std::move(heartbeat));
	}

  private:
	Network<NodeRole::STORAGE> &network_;
	NodeId metadata_node_;
};

#endif // STORAGEHEART_HPP