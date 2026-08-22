#ifndef RAFTRPC_HPP
#define RAFTRPC_HPP
#include "network/Node.hpp"
#include "rpc/RaftProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <variant>
#include "MailboxService.hpp"

using RaftEvent = std::variant<RaftRpcMessage, Stop>;

class RaftService : public MailboxService<RaftEvent, RaftService> {
  public:
	std::optional<NodeId> get_leader() {
		return current_leader_;
	}

	bool is_leader() {
		return self_id_ == current_leader_;
	}

  private:
	std::optional<NodeId> current_leader_;
	NodeId self_id_;
};

#endif // RAFTRPC_HPP