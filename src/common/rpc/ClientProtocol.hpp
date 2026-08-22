/**

Client--Metadata RPC

*/
#ifndef CLIENTPROTOCOL_HPP
#define CLIENTPROTOCOL_HPP
#include <cstdint>
#include <optional>
#include "Rpc.hpp"
#include "network/Node.hpp"

enum class ClientJob : std::uint8_t {
	CREATE_FILE,
	READ_FILE,
	WRITE_FILE,
	REMOVE,
	LIST,
	MKDIR,
	RENAME,
	STAT,

	NOT_LEADER = 0xFF, //
};

using ClientRpcMessage = RpcMessage<ClientJob>;
using ClientRpcHeader = RpcHeader<ClientJob>;

template <>
struct RpcTraits<ClientJob> {
	static constexpr MessageType message_type = MessageType::CLIENT_RPC;
};

enum class ClientStatus : std::uint8_t {
	Success = 0,

	InputError = 2,
};

struct RequestContext {
	std::uint64_t request_id;
	NodeIdentity sender;
};

struct CreateFileRequest {
	std::string path;
	RequestContext context;
};
struct CreateFileResponse {
	ClientStatus status;
};

struct LeaderHintRequest {
	RequestContext context;
};
struct LeaderHintResponse {
	std::optional<NodeId> leader_id;
};

namespace ClientProtocol {

Frame encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request);
CreateFileRequest decode_create_file_request(const ClientRpcMessage &message);

Frame encode_create_file_response(std::uint64_t request_id, const CreateFileResponse &response);
CreateFileResponse decode_create_file_response(const ClientRpcMessage &message);

Frame encode_leader_hint_response(std::uint64_t request_id, const LeaderHintResponse &response);
LeaderHintResponse decode_leader_hint_response(const ClientRpcMessage &message);

} // namespace ClientProtocol

#endif // CLIENTPROTOCOL_HPP