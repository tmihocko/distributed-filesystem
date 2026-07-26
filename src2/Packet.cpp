#include "Packet.hpp"
#include <cstddef>
#include <span>

std::vector<std::byte> &PacketWriter::data() {
	return buffer_;
}

std::uint32_t PacketWriter::length() const {
	return static_cast<std::uint32_t>(buffer_.size());
}

std::vector<std::byte> PacketReader::read_bytes(std::size_t n) {
	if (offset_ + n > data_.size()) {
		throw std::out_of_range("PacketReader::read_bytes: buffer underrun");
	}
	std::vector<std::byte> out(data_.begin() + offset_, data_.begin() + offset_ + n);
	offset_ += n;
	return out;
}