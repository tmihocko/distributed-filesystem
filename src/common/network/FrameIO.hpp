#ifndef FRAME_IO_HPP
#define FRAME_IO_HPP

#include "network/Message.hpp"
#include <asio.hpp>

namespace FrameIO {
using ReadHandler = std::function<void(asio::error_code, std::optional<Frame>)>;

using WriteHandler = std::function<void(asio::error_code, std::size_t)>;

// Blocking version. Mainly useful for tests and simple tools.
Frame read_frame(asio::ip::tcp::socket &socket);

void async_read_frame(asio::ip::tcp::socket &socket, ReadHandler handler);

void async_write_frame(asio::ip::tcp::socket &socket, Frame frame, WriteHandler handler);

} // namespace FrameIO

#endif // FRAME_IO_HPP