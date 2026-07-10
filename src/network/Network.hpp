#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "cluster/NodeInfo.hpp"
#include "util/Singleton.hpp"
#include <asio.hpp>
#include <span>

namespace Events {

}

class Network : public Singleton<Network> {
  public:
	void set_address(std::string host, unsigned int port);

	void find_peers(std::span<std::pair<std::string, unsigned int>> seed_nodes);

	/// Like expose stuff remote calls here

  private:
	std::string _host;
	unsigned int _port;

	asio::error_code _ec;
	asio::io_context _context;

	std::vector<NodeInfo> _peers;

	std::unordered_map<std::string, asio::ip::tcp::endpoint> _endpoints;
	std::unordered_map<std::string, asio::ip::tcp::socket> _sockets;

	std::string _hash_key(std::string host, unsigned int port);

	void listen(std::string host, unsigned int port);
};

#endif