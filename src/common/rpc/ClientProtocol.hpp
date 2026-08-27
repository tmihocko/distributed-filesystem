/**

Client--Metadata RPC

*/
#ifndef CLIENTPROTOCOL_HPP
#define CLIENTPROTOCOL_HPP
#include <cstdint>
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

};

enum class ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
	BadResponse,
	NotImplemented,
};

struct FileInfo {
	std::string path;
	bool is_directory;
};

struct FileStats {};

using ClientRpcMessage = RpcMessage<ClientJob>;
using ClientRpcHeader = RpcHeader<ClientJob>;

template <>
struct RpcTraits<ClientJob> {
	static constexpr MessageType message_type = MessageType::CLIENT_RPC;
};

enum class ClientStatus : std::uint8_t {
	Success = 0,
	ServerError = 1,
	InputError = 2,
	AlreadyExists = 3,
};

struct RequestContext {
	std::uint64_t request_id;
	NodeIdentity sender;

	template <typename T>
	static RequestContext from(const RpcMessage<T> &message) {
		return RequestContext{
			.request_id = message.rpc_header.request_id,
			.sender = message.sender,
		};
	}
};

struct CreateFileRequest {
	std::string path;
	RequestContext context;
};
struct CreateFileResponse {
	ClientStatus status;
	RequestContext context;
};

struct ListRequest {
	std::string path;
	RequestContext context;
};

// Rpc messages have uint32 size before
struct ListResponse {
	std::vector<FileInfo> info_vec;
	RequestContext context;
};

namespace ClientProtocol {

Frame encode_create_file_request(std::uint64_t request_id, const CreateFileRequest &request);
CreateFileRequest decode_create_file_request(const ClientRpcMessage &message);

Frame encode_create_file_response(std::uint64_t request_id, const CreateFileResponse &response);
CreateFileResponse decode_create_file_response(const ClientRpcMessage &message);

//

Frame encode_list_request(std::uint64_t request_id, const ListRequest &response);
ListRequest decode_list_request(const ClientRpcMessage &message);

Frame encode_list_response(std::uint64_t request_id, const ListResponse &response);
ListResponse decode_list_response(const ClientRpcMessage &message);

} // namespace ClientProtocol

#endif // CLIENTPROTOCOL_HPP