#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "util/Singleton.hpp"
#include <asio.hpp>

class Network : public Singleton<Network> {
  public:
	void start(std::string host, unsigned int port);

	/// Like expose stuff events here or something,

  private:
	bool _started = false;

	std::string _host;
	unsigned int _port;

	asio::error_code _ec;
	asio::io_context _context;
	asio::ip::tcp::socket *_socket;
};

#endif