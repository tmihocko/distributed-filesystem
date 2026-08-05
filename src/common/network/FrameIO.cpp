#include "FrameIO.hpp"
#include "Packet.hpp"
#include "network/Message.hpp"
#include <asio.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

namespace {

MessageHeader decode_header(std::span<const std::byte> bytes) {
	PacketReader reader(bytes);

	const auto [magic, payload_size, message_type] = reader.read<std::uint8_t, std::uint32_t, MessageType>();

	if (magic != HEADER_MAGIC) {
		throw std::runtime_error("Wrong frame magic.");
	}

	if (payload_size > MAX_MESSAGE_SIZE) {
		throw std::runtime_error(
			"Frame payload exceeds maximum size.");
	}

	return MessageHeader{
		.magic = magic,
		.length = payload_size,
		.type = message_type,
	};
}

std::vector<std::byte> encode_frame(const Frame &frame) {
	if (frame.buffer.size() > MAX_MESSAGE_SIZE) {
		throw std::length_error(
			"Frame payload exceeds maximum size.");
	}

	PacketWriter writer;

	// Serialize each field individually. Do not rely on struct padding.
	writer
		.write<std::uint8_t>(HEADER_MAGIC)
		.write<std::uint32_t>(
			static_cast<std::uint32_t>(
				frame.buffer.size()))
		.write<MessageType>(frame.header.type)
		.write_bytes(frame.buffer);

	return writer.move_data();
}

} // namespace

namespace FrameIO {

Frame read_frame(asio::ip::tcp::socket &socket) {
	std::array<std::byte, MESSAGE_HEADER_SIZE> header_bytes{};

	asio::read(
		socket,
		asio::buffer(header_bytes));

	const MessageHeader header =
		decode_header(header_bytes);

	std::vector<std::byte> payload(header.length);

	if (!payload.empty()) {
		asio::read(
			socket,
			asio::buffer(payload));
	}

	return Frame{
		.header = header,
		.buffer = std::move(payload),
	};
}

void async_read_frame(
	asio::ip::tcp::socket &socket,
	ReadHandler handler) {
	struct ReadState {
		std::array<std::byte, MESSAGE_HEADER_SIZE>
			header_bytes{};

		MessageHeader header{};
		std::vector<std::byte> payload;
	};

	auto state = std::make_shared<ReadState>();

	asio::async_read(
		socket,
		asio::buffer(state->header_bytes),
		[&socket,
		 state,
		 handler = std::move(handler)](
			const asio::error_code &error,
			std::size_t) mutable {
			if (error) {
				handler(error, std::nullopt);
				return;
			}

			try {
				state->header =
					decode_header(state->header_bytes);

				state->payload.resize(
					state->header.length);
			} catch (...) {
				handler(
					std::make_error_code(
						std::errc::protocol_error),
					std::nullopt);

				return;
			}

			if (state->payload.empty()) {
				handler(
					{},
					Frame{
						.header = state->header,
						.buffer =
							std::move(state->payload),
					});

				return;
			}

			asio::async_read(
				socket,
				asio::buffer(state->payload),
				[state,
				 handler = std::move(handler)](
					const asio::error_code &error,
					std::size_t) mutable {
					if (error) {
						handler(error, std::nullopt);
						return;
					}

					handler(
						{},
						Frame{
							.header = state->header,
							.buffer =
								std::move(state->payload),
						});
				});
		});
}

void async_write_frame(
	asio::ip::tcp::socket &socket,
	Frame frame,
	WriteHandler handler) {
	std::vector<std::byte> encoded;

	try {
		encoded = encode_frame(frame);
	} catch (...) {
		asio::post(
			socket.get_executor(),
			[handler = std::move(handler)]() mutable {
				handler(
					std::make_error_code(
						std::errc::message_size),
					0);
			});

		return;
	}

	// async_write does not copy the buffer. The shared_ptr keeps the
	// bytes alive until the completion handler runs.
	auto bytes = std::make_shared<std::vector<std::byte>>(std::move(encoded));

	asio::async_write(
		socket,
		asio::buffer(*bytes),
		[bytes,
		 handler = std::move(handler)](
			const asio::error_code &error,
			std::size_t written) mutable {
			handler(error, written);
		});
}

} // namespace FrameIO