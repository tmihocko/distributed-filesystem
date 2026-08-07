#ifndef CLIENTNETWORK_HPP
#define CLIENTNETWORK_HPP
#include <asio.hpp>
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "util/BlockingQueue.hpp"
#include "util/Singleton.hpp"
#include <atomic>
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
	IO_ERROR,
	TIMEOUT,
	PROTOCOL_ERROR,
	REDIRECT_LIMIT,
	SHUTTING_DOWN,
};

struct TransportError {
	TransportErrorCode code;
	std::string what;
};

struct ReadResult {
	std::uint64_t operation_id;
	std::error_code error;
	std::optional<Frame> frame;
};

struct ConnectResult {
	std::uint64_t operation_id;
	std::error_code error;
	Endpoint endpoint;
};

template <typename T>
using TransportOperation = std::expected<T, TransportError>;

class ClientTransport : public Singleton<ClientTransport> {

	friend class Singleton<ClientTransport>;

  public:
	~ClientTransport();

	TransportOperation<void> connect(std::span<const Endpoint> seed_nodes);

	TransportOperation<ClientProtocol::RpcResponse> request(ClientProtocol::Operation operation, std::vector<std::byte> payload);

	void shutdown();

  private:
	ClientTransport();

	asio::io_context context_;
	using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

	ClientProtocol::ClientSessionId session_id_;
	std::optional<Endpoint> current_leader_;
	std::vector<Endpoint> seed_nodes_; // Other cluster nodes

	std::optional<Endpoint> connected_endpoint_;
	WorkGuard guard_{ asio::make_work_guard(context_) };
	asio::ip::tcp::socket socket_{ context_ };
	asio::ip::tcp::resolver resolver_{ context_ };
	std::jthread io_thread_;

	BlockingQueue<ReadResult> read_results_;
	BlockingQueue<ConnectResult> connect_results_;

	TransportOperation<void> connect_to(const Endpoint &endpoint);
	void close_socket();
	TransportOperation<void> discover_nodes();
	TransportOperation<ClientProtocol::RpcResponse> send_and_wait(const Endpoint &leader, const ClientProtocol::RpcRequest &request);
	TransportOperation<Frame> perform_frame_round_trip(Frame outgoing);

	std::uint64_t next_io_operation_id_{ 1 };
	ClientProtocol::RequestId next_request_id_{ 1 };
	std::atomic_bool shutting_down_{ false };
};

#endif // CLIENTNETWORK_HPP