#ifndef NETWORK_HPP
#define NETWORK_HPP
#include "Message.hpp"
#include "Node.hpp"
#include "PeerStatus.hpp"
#include "asio/ip/tcp.hpp"
#include "util/Singleton.hpp"
#include "util/BlockingQueue.hpp"
#include <chrono>
#include <asio.hpp>
#include <thread>
#include <unordered_map>

enum class NetworkStatus {
	HANDSHAKING,
	ACTIVE,
	OFF,
};

class Network : public Singleton<Network> {
  public:
	void set_heartbeat_timer(std::chrono::steady_clock::time_point *last_time, std::chrono::milliseconds timeout);

	void run();

	// Receive the next message in message queue
	std::optional<Message> receive_with_timeout();

	void accept_peers();
	void find_peers(const std::string &config_file);

	std::size_t send(const Endpoint &endpoint, const Message &msg);
	std::size_t send_to_leader(const Message &msg);

	void shutdown();

	bool listening();

	std::optional<NodeInfo> leader();

  private:
	Message read_frame(asio::ip::tcp::socket &socket);
	void async_read_frame(
		asio::ip::tcp::socket &socket,
		std::function<void(std::error_code, std::optional<Message>)> handler);

	void start_reading(asio::ip::tcp::socket &socket);

	std::optional<PeerStatus> request_peer_status(const Endpoint &endpoint);

	asio::io_context context_;
	asio::ip::tcp::acceptor acceptor_{ context_ };

	NetworkStatus status_ = NetworkStatus::HANDSHAKING;

	BlockingQueue<ReceivedMessage> message_queue_;
	std::chrono::steady_clock::time_point *last_heartbeat_;
	std::chrono::milliseconds timeout_;
	std::jthread network_thread_;

	NodeInfo self_ = NodeInfo{};
	std::optional<NodeInfo> leader_;
	std::unordered_map<std::string, asio::ip::tcp::socket> connections_; // { endpoint.key : socket }
	std::unordered_map<NodeId, NodeInfo> peers_;
};

#endif // NETWORK_HPP