# Todo

### Hiding internals for `#include "Client.hpp"`

Client.hpp exposes internal types:

```cpp
#include "network/Network.hpp"
#include "network/Node.hpp"

class Client {
    Network<NodeRole::CLIENT> network_;
};
```

Therefore, anyone including Client.hpp must also parse and have access to your common/network headers.
Use the PImpl pattern to hide that.
Public header:

```cpp
// src/client/include/dfs/Client.hpp
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace dfs {

struct Endpoint {
	std::string host;
	std::uint16_t port;
};

enum class ClientError : std::uint8_t {
	AlreadyExists,
	ServerError,
	BadResponse,
	NotImplemented,
};

template <typename T>
using ClientOperation = std::expected<T, ClientError>;

class Client {
  public:
	Client(Endpoint self, std::span<const Endpoint> seed_nodes);
	~Client();

	Client(Client &&) noexcept;
	Client &operator=(Client &&) noexcept;

	Client(const Client &) = delete;
	Client &operator=(const Client &) = delete;

	ClientOperation<void> create_file(std::string path);

  private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace dfs
```

This header contains only the user-facing API. It does not mention Network, RPC headers, node roles, packet readers, or queues.
In Client.cpp, define the internals:

```cpp
#include <dfs/Client.hpp>

#include "network/Network.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"

namespace dfs {

class Client::Impl {
  public:
	// Network, leader ID, request counter, etc.
	Network<NodeRole::CLIENT> network_;
	std::optional<NodeId> leader_;
	std::uint64_t current_id_ = 0;
};

Client::~Client() = default;
Client::Client(Client &&) noexcept = default;
Client &Client::operator=(Client &&) noexcept = default;

// Other implementations...

} // namespace dfs
```

Then your client CMake becomes:

```cmake
add_library(DFS_Client
    Client.cpp
)

target_include_directories(DFS_Client
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(DFS_Client
    PRIVATE DFS_Common
)
```
