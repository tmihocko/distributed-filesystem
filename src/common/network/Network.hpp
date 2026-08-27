#ifndef NETWORK_HPP
#define NETWORK_HPP
#include <asio.hpp>
#include "network/Connection.hpp"
#include "network/Message.hpp"
#include <asio.hpp>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "network/Node.hpp"
#include "util/BlockingQueue.hpp"

using ConnectionPtr = std::shared_ptr<Connection>;

template <NodeRole SelfRole>
class Network {
  public:
	Network(Endpoint self) : self_(self) {
		assert(self.role == SelfRole);
	}

	Network(NodeConfig self) {
		self_ = Endpoint{
			.node_id = self.node_id,
			.role = self.role,
			.host = self.host,
			.port = self.port,
		};
		assert(self.role == SelfRole);
	}

	void start(std::span<const Endpoint> seed_nodes);

	void send(NodeId node_id, Frame frame);

	void broadcast(NodeRole role, Frame frame);

	Message receive();

	template <typename Rep, typename Period>
	std::optional<Message> receive(std::chrono::duration<Rep, Period> timeout);

	auto nodes_of_role(NodeRole role);

	static constexpr NodeRole self_role = SelfRole;

	Network(const Network &) = delete;
	auto operator=(const Network &) = delete;
	Network(Network &&) = delete;
	auto operator=(Network &&) = delete;

  private:
	// By role
	static constexpr bool can_connect_with_role(NodeRole role);

	// Removes duplicate and incorrect connections,
	bool should_connect(const Endpoint &endpoint);

	// Attempt to connect with endpoint, does not handshake
	void connect(const Endpoint &endpoint);

	// Start accepting incoming connections \
	// Runs in io_context
	void start_accepting();

	// Does not create a TCP connection. It records a connection that has already connected and completed its handshake
	void register_connection(ConnectionPtr connection, NodeIdentity remote);

	void start_reading(ConnectionPtr connection);

	void handshake(ConnectionPtr connection);

	void fail_connection(ConnectionPtr connection);
	void schedule_reconnect(const Endpoint &endpoint);

	asio::io_context context_;
	asio::ip::tcp::acceptor acceptor_{ context_ };

	BlockingQueue<Message> message_queue_;
	std::jthread network_thread_;

	Endpoint self_;
	std::unordered_map<NodeId, ConnectionPtr> connections_;
	std::unordered_set<ConnectionPtr> pending_connections_;
	std::unordered_map<NodeId, std::deque<Frame>> pending_frames_; // Frames passed by send() before connections with node id has been made
};

#ifndef NETWORK_TPP
#include "Network.tpp"
#endif
#endif // NETWORK_HPP