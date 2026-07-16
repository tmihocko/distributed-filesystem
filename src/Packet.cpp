#include "Packet.hpp"
#include <span>

std::span<const std::byte> PacketWriter::data() const {
	return buffer_;
}

std::vector<std::byte> PacketReader::read_bytes(std::size_t n) {
	if (offset_ + n > data_.size()) {
		throw std::out_of_range("PacketReader::read_bytes: buffer underrun");
	}
	std::vector<std::byte> out(data_.begin() + offset_, data_.begin() + offset_ + n);
	offset_ += n;
	return out;
}