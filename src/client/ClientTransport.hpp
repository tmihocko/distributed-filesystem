#ifndef CLIENTNETWORK_HPP
#define CLIENTNETWORK_HPP
#include <asio.hpp>
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "util/BlockingQueue.hpp"
#include "util/Singleton.hpp"
#include <expected>
#include <span>
#include <system_error>
#include <thread>
#include "protocol/ClientProtocol.hpp"

// The client should only speak to the leader, this handles all endpoint stuff
// Without it we'd need manually to check when a new leader is elected and also who
// the leader is on startup
enum class TransportErrorCode {
	NO_SEEDS,
	NOT_CONNECTED,
	CONNECT_FAILED,
	NOT_IMPLEMENTED,
	PROTOCOL_ERROR,
	REDIRECT_LIMIT,
};

struct TransportError {
	TransportErrorCode code;
	std::string what;
};

struct ReadResult {
	std::error_code error;
	std::optional<Frame> frame;
};

struct ConnectResult {
	std::error_code error;
	Endpoint endpoint;
};

class ClientTransport : public Singleton<ClientTransport> {

	friend class Singleton<ClientTransport>;

  public:
	~ClientTransport();

	std::expected<void, TransportError> connect(std::span<const Endpoint> seed_nodes);

	std::expected<ClientProtocol::RpcResponse, TransportError> request(ClientProtocol::Operation operation, std::vector<std::byte> payload);

	void shutdown();

  private:
	ClientTransport();

	asio::io_context context_;
	using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

	ClientProtocol::ClientSessionId session_id_;
	std::optional<Endpoint> current_leader_;
	std::vector<Endpoint> seed_nodes_; // Other cluster nodes

	WorkGuard guard_{ asio::make_work_guard(context_) };
	asio::ip::tcp::socket socket_{ context_ };
	std::jthread io_thread_;

	BlockingQueue<ReadResult> read_results_;
	BlockingQueue<ConnectResult> connect_results_;

	std::deque<std::shared_ptr<std::vector<std::byte>>> outgoing_writes_;

	std::expected<void, TransportError> discover_leader();
	std::expected<ClientProtocol::RpcResponse, TransportError> send_and_wait(const Endpoint &leader, const ClientProtocol::RpcRequest &request);

	ClientProtocol::RequestId next_request_id_{ 1 };
	bool shutting_down_ = false;
};

#endif // CLIENTNETWORK_HPP