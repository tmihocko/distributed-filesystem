#ifndef METADATARPC_HPP
#define METADATARPC_HPP
#include "network/Packet.hpp"
#include "rpc/ClientProtocol.hpp"
#include "RpcService.hpp"
#include "rpc/Rpc.hpp"
#include <stdexcept>
#include <variant>

using ClientEvent = std::variant<ClientRpcMessage, Stop>;

class ClientRpc : public RpcService<ClientEvent, ClientRpc> {
  public:
	void handle(ClientRpcMessage message) {
		if (true) {				   // if (raft_.is_leader()) {
			PacketWriter writer{}; // { raft_.leader_id() };
			Frame frame = Rpc::make_frame(message.rpc_header.request_id, ClientJob::LEADER_HINT, RpcKind::Response, writer.move_data());

			// network_.send(message.sender, frame)
		}

		switch (message.rpc_header.job) {
		case ClientJob::CREATE_FILE: {
			create_file(PacketReader{ message.body }.read_string());
			break;
		}
		default:
			throw std::runtime_error("[ClientRpc]: clientjob not handled");
		}
	}

  private:
	void create_file(std::string path) {}
};

#endif // METADATARPC_HPP