#include "Network.hpp"

void Network::start(std::string host, unsigned int port) {
	assert(_started == false);
	asio::ip::tcp::endpoint endpoint(asio::ip::make_address(host, _ec), port);

	asio::ip::tcp::socket socket(_context);

	socket.connect(endpoint, _ec);

	_started = true;
}