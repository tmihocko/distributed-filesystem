#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
concept BinarySerializable = std::is_trivially_copyable_v<T> || std::is_same_v<std::string, T>;
class BinaryWriter {
  public:
	BinaryWriter() = default;

	template <BinarySerializable... Ts>
		requires(sizeof...(Ts) > 1)
	BinaryWriter write(const Ts &...values) {
		(write(values), ...);
		return *this;
	}

	template <BinarySerializable T>
	BinaryWriter &write(const T &value) {
		if constexpr (std::is_same_v<std::string, T>) {
			return write_string(value);
		} else {
			assert_valid();

			auto old = buffer_.size();
			buffer_.resize(old + sizeof(T));
			std::memcpy(buffer_.data() + old, &value, sizeof(T));

			return *this;
		}
	}

	// Writes until null terminator
	// Includes null terminator in message, maybe remove that later, will see
	BinaryWriter &write_string(const std::string &value) {
		assert_valid();
		const std::size_t byte_count = value.size() + 1;
		const std::size_t old = buffer_.size();

		buffer_.resize(old + byte_count);
		std::memcpy(buffer_.data() + old, value.c_str(), byte_count);

		return *this;
	}

	BinaryWriter &write_bytes(std::span<const std::byte> bytes) {
		assert_valid();

		buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());

		return *this;
	}

	[[nodiscard]] std::vector<std::byte> move_data();
	std::span<const std::byte> data();
	std::uint32_t length() const;

  private:
	void assert_valid() const;

	std::vector<std::byte> buffer_;
	bool valid_ = true;
};

class BinaryReader {
  public:
	BinaryReader(std::span<const std::byte> bytes) : data_(bytes) {}

	template <BinarySerializable T>
	T read() {
		if constexpr (std::is_same_v<std::string, T>) {
			return read_string();
		} else {
			if (offset_ > data_.size() || sizeof(T) > data_.size() - offset_) {
				throw std::out_of_range("PacketReader buffer underrun");
			}

			T value;
			std::memcpy(&value, data_.data() + offset_, sizeof(T));
			offset_ += sizeof(T);
			return value;
		}
	};

	template <BinarySerializable... Ts>
		requires(sizeof...(Ts) > 1)
	std::tuple<Ts...> read() {
		return std::tuple<Ts...>{ read<Ts>()... };
	}

	std::string read_string() {
		const auto begin = data_.begin() + offset_;
		const auto null_terminator = std::find(begin, data_.end(), std::byte{ 0 });

		if (null_terminator == data_.end()) {
			throw std::runtime_error("Packet string has no null terminator");
		}

		const auto length = static_cast<std::size_t>(std::distance(begin, null_terminator));
		const char *chars = reinterpret_cast<const char *>(data_.data() + offset_);

		std::string value(chars, length);
		offset_ += length + 1;

		return value;
	}

	std::vector<std::byte> read_remaining();
	std::vector<std::byte> read_bytes(std::size_t n);

	std::size_t remaining() const noexcept;
	bool at_end() const noexcept;
	void assert_at_end() const;

  private:
	std::span<const std::byte> data_;
	std::size_t offset_ = 0;
};

#endif