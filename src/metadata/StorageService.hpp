#ifndef STORAGERPC_HPP
#define STORAGERPC_HPP
#include <variant>
#include "rpc/StorageProtocol.hpp"
#include "MailboxService.hpp"

using StorageEvent = std::variant<StorageRpcMessage, Stop>;

class StorageService : public MailboxService<StorageEvent, StorageService> {
  public:
  private:
};

#endif // STORAGERPC_HPP