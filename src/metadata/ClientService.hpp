#ifndef METADATARPC_HPP
#define METADATARPC_HPP
#include "MailboxService.hpp"
#include "RaftService.hpp"
#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include <variant>

using ClientEvent = std::variant<CreateFileRequest, LeaderHintRequest, Stop>;

class ClientService : public MailboxService<ClientEvent, ClientService> {
  public:
	void handle(CreateFileRequest req) {
		auto path = req;

		// do_stuff(path);

		// ClientProtocol
	}

	// Send a leader hint to the client
	void handle(LeaderHintRequest req) {
		auto leader = raft_.get_leader();

		LeaderHintResponse response{ std::move(leader) };
		auto req_id = req.message.rpc_header.request_id;

		auto frame = ClientProtocol::encode_leader_hint_response(req_id, response);

		network_.send(req.message.sender.id, std::move(frame));
	}

	ClientService(auto &network) : network_(network) {}

  private:
	Network<NodeRole::METADATA> &network_;
	RaftService raft_;
	// inject other services
};

#endif // METADATARPC_HPP