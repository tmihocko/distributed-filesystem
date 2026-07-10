#ifndef PACKET_HPP
#define PACKET_HPP
#include <span>
#include <type_traits>
#include <vector>

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

class PacketWriter {
  public:
	template <TriviallyCopyable T>
	void write(const T &value) {
		auto old = buffer.size();
		buffer.resize(old + sizeof(T));

		std::memcpy(buffer.data() + old, &value, sizeof(T));
	}

	std::span<const std::byte> data() const {
		return buffer;
	}

  private:
	std::vector<std::byte> buffer;
};

class PacketReader {
  public:
	PacketReader(std::span<const std::byte> bytes) : data(bytes) {}

	template <TriviallyCopyable T>
	T read() {
		T value;
		std::memcpy(&value, data.data() + offset, sizeof(T));
		offset += sizeof(T);
		return value;
	};

	std::vector<std::byte> read_bytes(size_t n) {
		if (offset + n > data.size()) {
			throw std::out_of_range("PacketReader::read_bytes: buffer underrun");
		}
		std::vector<std::byte> out(data.begin() + offset, data.begin() + offset + n);
		offset += n;
		return out;
	}

  private:
	std::span<const std::byte> data;
	std::size_t offset = 0;
};

#endif