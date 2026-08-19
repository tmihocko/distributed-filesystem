#ifndef NUMBER_CONVERSION_HPP
#define NUMBER_CONVERSION_HPP

#include <charconv>
#include <concepts>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>

/**
 * std::from_chars only accepts narrow character ranges
 * const char* not wchar_t*, char16_t*, or char32_t*.
 */
template <typename T>
concept NumericInteger =
	std::integral<T> &&
	!std::same_as<std::remove_cv_t<T>, bool> &&
	!std::same_as<std::remove_cv_t<T>, char> &&
	!std::same_as<std::remove_cv_t<T>, signed char> &&
	!std::same_as<std::remove_cv_t<T>, unsigned char> &&
	!std::same_as<std::remove_cv_t<T>, wchar_t> &&
	!std::same_as<std::remove_cv_t<T>, char8_t> &&
	!std::same_as<std::remove_cv_t<T>, char16_t> &&
	!std::same_as<std::remove_cv_t<T>, char32_t>;

template <NumericInteger T>
[[nodiscard]] T string_to_int(std::string_view text, int base = 10) {

	T value{};

	const char *begin = text.data();
	const char *end = begin + text.size();

	const auto [ptr, error] = std::from_chars(begin, end, value, base);

	if (error == std::errc::invalid_argument) {
		throw std::invalid_argument("Invalid integer");
	}

	if (error == std::errc::result_out_of_range) {
		throw std::out_of_range("Integer is outside the target type's range");
	}

	if (ptr != end) {
		throw std::invalid_argument("Unexpected characters after integer");
	}

	return value;
}

#endif // NUMBERCONVERSION_HPP
