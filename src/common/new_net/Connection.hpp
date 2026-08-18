#ifndef CONNECTION_HPP
#define CONNECTION_HPP
#include <asio.hpp>
#include "asio/error_code.hpp"
#include "network/FrameIO.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <queue>

enum class ConnectionOrigin {
	Incoming,
	Outgoing
};

class Connection : public std::enable_shared_from_this<Connection> {
  public:
	using FailureHandler = std::function<void(std::shared_ptr<Connection>, asio::error_code)>;

	Connection(asio::ip::tcp::socket socket, ConnectionOrigin origin, FailureHandler failure_handler) : socket(std::move(socket)), origin(origin), failure_handler_(std::move(failure_handler)) {}

	asio::ip::tcp::socket socket;
	ConnectionOrigin origin;
	std::optional<NodeIdentity> remote;
	std::optional<NodeIdentity> expected_remote;

	void enqueue_write(Frame frame) {
		write_queue_.push(std::move(frame));

		if (!write_in_progress_) {
			write_next();
		}
	}

	void close() {
		if (closed_)
			return;

		closed_ = true;
		write_in_progress_ = false;

		std::queue<Frame> empty;
		write_queue_.swap(empty);

		asio::error_code ignored;
		socket.cancel(ignored);
		socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
		socket.close(ignored);
	}

  private:
	FailureHandler failure_handler_;

	std::queue<Frame> write_queue_;
	bool write_in_progress_ = false;
	bool closed_ = false;

	void write_next() {
		if (closed_) return;

		if (write_queue_.empty()) {
			write_in_progress_ = false;
			return;
		}

		write_in_progress_ = true;

		Frame frame = std::move(write_queue_.front());
		write_queue_.pop();

		FrameIO::async_write_frame(socket, frame, [self = shared_from_this()](const asio::error_code &error, std::size_t) {
			if (self->closed_) return;

			if (error) {
				self->write_in_progress_ = false;

				self->failure_handler_(self, error);
				return;
			}

			self->write_next();
		});
	}
};

struct Hello {
	NodeIdentity identity;
	std::optional<Endpoint> advertised_endpoint;
};

#endif // CONNECTION_HPP