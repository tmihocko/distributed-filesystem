#include "Serializer.hpp"
#include <cstddef>
#include <span>
#include <stdexcept>

std::span<const std::byte> BinaryWriter::data() {
	assert_valid();
	return buffer_;
}

std::vector<std::byte> BinaryWriter::move_data() {
	assert_valid();
	valid_ = false;
	return std::move(buffer_);
}

std::uint32_t BinaryWriter::length() const {
	assert_valid();
	return static_cast<std::uint32_t>(buffer_.size());
}

void BinaryWriter::assert_valid() const {
	if (!valid_) throw std::logic_error("Writer has been consumed.");
}

std::vector<std::byte> BinaryReader::read_bytes(std::size_t n) {
	if (offset_ + n > data_.size()) {
		throw std::out_of_range("PacketReader::read_bytes: buffer underrun");
	}
	std::vector<std::byte> out(data_.begin() + offset_, data_.begin() + offset_ + n);
	offset_ += n;
	return out;
}

std::vector<std::byte> BinaryReader::read_remaining() {
	return read_bytes(remaining());
};

std::size_t BinaryReader::remaining() const noexcept {
	return data_.size() - offset_;
}

bool BinaryReader::at_end() const noexcept {
	return offset_ == data_.size();
}

void BinaryReader::assert_at_end() const {
	if (!at_end()) throw std::runtime_error("Data not at end");
}