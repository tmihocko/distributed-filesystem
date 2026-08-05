#ifndef CLIENT_HPP
#define CLIENT_HPP
#include "network/Node.hpp"
#include "util/Singleton.hpp"
#include <expected>
#include <string>

enum ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
};

struct FileInfo {};

enum class Operation : std::uint8_t {
	CREATE_FILE,
	READ_FILE,
	WRITE_FILE,
	REMOVE,
	LIST,
	MKDIR,
	RENAME,
	STAT,
};

class Client final : public Singleton<Client> {
  public:
	std::expected<void, ClientError> connect(const std::vector<Endpoint> &seed_nodes);

	std::expected<void, ClientError> create_file();

	std::expected<void, ClientError> read_file();

	std::expected<void, ClientError> write_file();

	std::expected<void, ClientError> list();

	std::expected<void, ClientError> mkdir(std::string path);

	std::expected<void, ClientError> rename();

	std::expected<void, ClientError> stat();

	// other rpc calls

  private:
	ClientTransport &transport_ = ClientTransport::get();
};

#endif // CLIENT_HPP