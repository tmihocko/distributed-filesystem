#ifndef NETWORK_HPP
#define NETWORK_HPP
#include "Message.hpp"
#include "asio/ip/tcp.hpp"
#include "../util/Singleton.hpp"
#include "../util/BlockingQueue.hpp"
#include <asio.hpp>
#include <memory>
#include <thread>
#include <unordered_map>
#include "Node.hpp"
#include <unordered_set>

enum class NetworkStatus {
	HANDSHAKING,
	ACTIVE,
	OFF,
};

struct Hello {
	NodeIdentity identity;
	std::optional<Endpoint> advertised_endpoint;
};

struct Connection {
	explicit Connection(asio::io_context &context, bool outgoing) : socket(context), outgoing(outgoing) {}

	explicit Connection(asio::ip::tcp::socket socket, bool outgoing) : socket(std::move(socket)), outgoing(outgoing) {}

	asio::ip::tcp::socket socket;
	std::optional<NodeIdentity> remote;
	std::optional<NodeIdentity> expected_remote;
	bool outgoing;
};

class Network final : public Singleton<Network> {
  public:
	void run();

	// Receive the next message in message queue
	std::optional<Message> receive_with_timeout();

	void accept_peers();
	void find_peers(const std::string &config_file);
	void connect_to_peer(const Endpoint &peer);

	void async_send_frame(std::shared_ptr<Connection> connection, Frame frame);

	void shutdown();

  private:
	void schedule_reconnect(const Endpoint &peer);
	void register_connection(std::shared_ptr<Connection> connection, Hello hello);
	void unregister_connection(std::shared_ptr<Connection> connection);

	void start_reading(std::shared_ptr<Connection> connection);

	void send_hello(std::shared_ptr<Connection> connection);
	void read_hello(std::shared_ptr<Connection> connection);

	void close_pending(const std::shared_ptr<Connection> &connection);

	asio::io_context context_;
	asio::ip::tcp::acceptor acceptor_{ context_ };

	std::atomic<NetworkStatus> status_ = NetworkStatus::HANDSHAKING;

	BlockingQueue<Message> message_queue_;
	std::jthread network_thread_;

	Endpoint self_ = Endpoint{};
	std::unordered_map<NodeId, Endpoint> known_nodes_;
	std::unordered_set<std::shared_ptr<Connection>> pending_connections_;

	// { node_id : socket }
	std::unordered_map<NodeId, std::shared_ptr<Connection>> metadata_connections_;
	std::unordered_map<NodeId, std::shared_ptr<Connection>> storage_connections_;
	std::unordered_map<NodeId, std::shared_ptr<Connection>> client_connections_;
};

#endif // NETWORK_HPP