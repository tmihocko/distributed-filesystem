#include "Packet.hpp"
#include <cstddef>
#include <span>
#include <stdexcept>

std::span<std::byte> PacketWriter::data() {
	assert_valid();
	return buffer_;
}

std::vector<std::byte> PacketWriter::move_data() {
	assert_valid();
	valid_ = false;
	return std::move(buffer_);
}

std::uint32_t PacketWriter::length() const {
	return static_cast<std::uint32_t>(buffer_.size());
}

void PacketWriter::assert_valid() const {
	if (!valid_) throw std::logic_error("Writer has been consumed.");
}

std::vector<std::byte> PacketReader::read_bytes(std::size_t n) {
	if (offset_ + n > data_.size()) {
		throw std::out_of_range("PacketReader::read_bytes: buffer underrun");
	}
	std::vector<std::byte> out(data_.begin() + offset_, data_.begin() + offset_ + n);
	offset_ += n;
	return out;
}