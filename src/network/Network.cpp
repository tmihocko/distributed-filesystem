#include "Network.hpp"
#include "network/EventRegistry.hpp"

void Network::set_address(std::string host, unsigned int port) {
	_host = host;
	_port = port;
}

void Network::find_peers(std::span<std::pair<std::string, unsigned int>> seed_nodes) {
	for (const auto &[host, port] : seed_nodes) {
		std::string key = _hash_key(host, port);
		if (_endpoints.contains(key) || _sockets.contains(key)) continue;

		auto addr = asio::ip::make_address(host, _ec);
		if (_ec) continue;

		asio::ip::tcp::endpoint endpoint(addr, port);

		asio::ip::tcp::socket socket(_context);
		auto ec = socket.connect(endpoint, _ec);
		if (ec) continue;

		_endpoints.emplace(key, std::move(endpoint));
		_sockets.emplace(key, std::move(socket));

		listen(host, port);
	}
}

std::string Network::_hash_key(std::string host, unsigned int port) {
	return host + std::to_string(port);
}

void Network::listen(std::string host, unsigned int port) {
	auto &socket = _sockets.at(_hash_key(host, port));
	auto header_buffer = std::make_shared<std::vector<std::byte>>(sizeof(MessageHeader));

	asio::async_read(socket, asio::buffer(*header_buffer), [this, &host, &socket, port, header_buffer](asio::error_code ec, std::size_t n) {
		if (ec) return;

		PacketReader reader(*header_buffer);
		auto header = reader.read<MessageHeader>();

		auto payload_buffer = std::make_shared<std::vector<std::byte>>(header.length);

		asio::async_read(socket, asio::buffer(*payload_buffer), [this, &host, port, header, payload_buffer](asio::error_code ec2, std::size_t n2) {
			if (ec2) return;

			EventRegistry::shared().dispatch({ header, *payload_buffer });

			listen(host, port);
		});
	});
}