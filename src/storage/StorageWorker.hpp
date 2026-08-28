#ifndef STORAGEWORKER_HPP
#define STORAGEWORKER_HPP

#include "rpc/StorageProtocol.hpp"
class StorageWorker {
  public:
	void handle(StorageRpcMessage) {}

  private:
}; // Does worker jobs

#endif // STORAGEWORKER_HPP