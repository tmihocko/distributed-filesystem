/**
TODO:!

Fix endianness of serialized bytes
*/
#ifndef SERIALIZER_HPP
#define SERIALIZER_HPP
#include <chrono>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
concept BinarySerializable =
	std::is_integral_v<T> ||
	std::is_enum_v<T> ||
	std::is_same_v<T, std::string> ||
	std::is_same_v<T, std::chrono::system_clock::time_point>;

class BinaryWriter {
  public:
	BinaryWriter() = default;

	template <BinarySerializable... Ts>
		requires(sizeof...(Ts) > 1)
	BinaryWriter &write(const Ts &...values) {
		(write(values), ...);
		return *this;
	}

	template <BinarySerializable T>
	BinaryWriter &write(const T &value) {
		if constexpr (std::is_same_v<std::string, T>) {
			return write_string(value);
		} else if constexpr (std::is_same_v<bool, T>) {
			return write<std::uint8_t>(value ? 1 : 0);
		} else if constexpr (std::is_same_v<std::chrono::system_clock::time_point, T>) {
			const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();

			return write<std::int64_t>(static_cast<std::int64_t>(milliseconds));
		} else if constexpr (std::is_integral_v<T>) {
			return write_integral(value);
		} else {
			assert_valid();

			auto old = buffer_.size();
			buffer_.resize(old + sizeof(T));
			std::memcpy(buffer_.data() + old, &value, sizeof(T));

			return *this;
		}
	}

	template <typename T>
		requires std::is_integral_v<T>
	BinaryWriter &write_integral(T value) {
		assert_valid();

		using Unsigned = std::make_unsigned_t<T>;

		Unsigned bits;

		if constexpr (std::is_signed_v<T>) {
			bits = std::bit_cast<Unsigned>(value);
		} else {
			bits = static_cast<Unsigned>(value);
		}

		for (std::size_t index = sizeof(T); index > 0; index--) {
			const std::size_t shift = (index - 1) * 8;

			buffer_.push_back(static_cast<std::byte>((bits >> shift) & static_cast<Unsigned>(0xff)));
		}

		return *this;
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
		} else if constexpr (std::is_same_v<bool, T>) {
			return read<std::uint8_t>() == 1;
		} else if constexpr (std::is_same_v<std::chrono::system_clock::time_point, T>()) {
			using namespace std::chrono;
			const auto ms = read<std::int64_t>();

			return system_clock::time_point{ duration_cast<system_clock::time_point::duration>(milliseconds{ ms }) };
		} else if constexpr (std::is_integral_v<T>) {
			return read_integral<T>();
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

	template <typename T>
		requires std::is_integral_v<T>
	T read_integral() {
		if (offset_ > data_.size() || sizeof(T) > data_.size() - offset_) {
			throw std::out_of_range("BinaryReader buffer underrun");
		}

		using Unsigned = std::make_unsigned_t<T>;

		Unsigned bits = 0;

		for (std::size_t index = 0; index < sizeof(T); ++index) {
			bits = static_cast<Unsigned>(bits << 8);
			bits |= static_cast<Unsigned>(std::to_integer<std::uint8_t>(data_[offset_ + index]));
		}

		offset_ += sizeof(T);

		if constexpr (std::is_signed_v<T>) {
			return std::bit_cast<T>(bits);
		} else {
			return static_cast<T>(bits);
		}
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