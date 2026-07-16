#ifndef NETWORK_HPP
#define NETWORK_HPP
#include "Message.hpp"
#include "NodeConfig.hpp"
#include "PeerStatus.hpp"
#include "util/Singleton.hpp"
#include <chrono>
#include <asio.hpp>
#include <queue>
#include <unordered_map>

class Network : public Singleton<Network> {
  public:
	void set_heartbeat_timer(std::chrono::steady_clock::time_point *last_time, std::chrono::milliseconds timeout);

	// Receive the next message in message queue
	Message receive_with_timeout();

	void set_self(NodeInfo node);

	void find_peers(const std::string &config_file);

	std::size_t send(const Endpoint &endpoint, const Message &msg);

	std::size_t send_leader(const Message &msg);

	void shutdown();

	bool listening();

	std::optional<NodeInfo> leader();

  private:
	std::optional<PeerStatus> request_peer_status(const Endpoint &endpoint);

	asio::io_context context_;

	std::queue<Message> message_queue_;

	bool listening_ = false;
	std::chrono::steady_clock::time_point *last_heartbeat_;
	std::chrono::milliseconds timeout_;

	NodeInfo self_ = NodeInfo{};
	std::optional<NodeInfo> leader_;
	std::unordered_map<std::string, asio::ip::tcp::socket> sockets_; // endpoint.key
	std::unordered_map<NodeId, NodeInfo> peers_;
};

#endif // NETWORK_HPP