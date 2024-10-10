#ifndef SEVFATE_ENDIANNESS_HPP
#define SEVFATE_ENDIANNESS_HPP
/**
 * @file
 * @brief Endianness converter functions.
 * Ported to mostly match the ETL implementation.
 */

#include <type_traits>

#if __cplusplus >= 202002L
    #include <bit>
#else
namespace std {
enum class endian {
    #if defined(_MSC_VER) && !defined(__clang__)
        little = 0,
        big = 1,
        native = little
    #else
        little = __ORDER_LITTLE_ENDIAN__,
        big = __ORDER_BIG_ENDIAN__,
        native = __BYTE_ORDER__
    #endif
};
}
#endif

namespace codec {

template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == 1U, T>
reverse_bytes(T value) {
    return value;
}
template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == 2U, T>
reverse_bytes(T value) {
    return (value >> 8U) | (value << 8U);
}
template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == 4U, T>
reverse_bytes(T value) {
    value = ((value & 0xFF00FF00UL) >> 8U) | ((value & 0x00FF00FFUL) << 8U);
    value = (value >> 16U) | (value << 16U);

    return value;
}
template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T> && sizeof(T) == 8U, T>
reverse_bytes(T value) {
    value = ((value & 0xFF00FF00FF00FF00ULL) >> 8U) | ((value & 0x00FF00FF00FF00FFULL) << 8U);
    value = ((value & 0xFFFF0000FFFF0000ULL) >> 16U) | ((value & 0x0000FFFF0000FFFFULL) << 16U);
    value = (value >> 32U) | (value << 32U);

    return value;
}
template<typename T>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, T> reverse_bytes(
     T value) {
    typedef typename std::make_unsigned<T>::type U;

    return static_cast<T>(reverse_bytes(static_cast<U>(value)));
}

template<typename T> constexpr std::enable_if_t<std::is_integral_v<T>, T> ntoh(T value) {
    if (std::endian::native == std::endian::little) {
        return reverse_bytes(value);
    }
    else {
        return value;
    }
}

template<typename T> constexpr std::enable_if_t<std::is_integral_v<T>, T> hton(T value) {
    if (std::endian::native == std::endian::little) {
        return reverse_bytes(value);
    }
    else {
        return value;
    }
}

}  // namespace codec

#endif /*SEVFATE_ENDIANNESS_HPP*/