#ifndef SEVFATE_TCODE_NUMERIC_HPP
#define SEVFATE_TCODE_NUMERIC_HPP

#include <utils/Class.hpp>
#include <utils/Trace.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace tcode {

template<typename T> class fractional {
    static_assert(std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed);

   protected:
    T _num = 0;
    T _denom = std::numeric_limits<T>::max();

   public:
    constexpr fractional() = default;
    constexpr fractional(T num, T denom) : _num(num), _denom(denom) {}
    /**
     * Construct by converting from string to integer.
     */
    fractional(const char* const begin, const char* const end) noexcept {
        if (!parse(begin, end)) {
            _num = 0;
            _denom = std::numeric_limits<T>::max();
        }
    }

    bool parse(const char* const begin, const char* const end) noexcept {
        return parse(begin, static_cast<uintptr_t>(end - begin));
    }
    bool parse(const char* const str, const size_t n) noexcept {
        if (n == 0 || n > std::numeric_limits<uint32_t>::digits10) [[unlikely]] {
            return false;
        }
        _num = 0;
        _denom = 0;
        for (size_t i = 0; i < n; i++) {
            uint8_t digit = str[i] - '0';
            if (digit > 9) [[unlikely]] {
                return false;
            }
            _num *= 10;
            _num += digit;
            _denom *= 10;
            _denom += 9;
        }
        return true;
    }

    T numerator() const {
        return _num;
    }
    T denominator() const {
        return _denom;
    }

    template<typename R> std::enable_if_t<std::is_floating_point_v<R>, R> quotient() const {
        return static_cast<R>(_num) / static_cast<R>(_denom);
    }

    // TODO: void to_ratio() const;

    operator float() const {
        return quotient<float>();
    }
    operator double() const {
        return quotient<double>();
    }
};

/** Make an integral of the specified type U, with \p n count of 9 digits. */
template<typename U> constexpr std::optional<U> make_nines(int n) {
    if (n <= 0 || n > std::numeric_limits<U>::digits10) [[unlikely]] {
        return {};
    }
    U denom = 0;
    for (int i = 0; i < n; i++) {
        denom *= 10;
        denom += 9;
    }
    return denom;
}
template<typename U, int n> sevf_meta_consteval_since_cpp20 U make_nines() {
    static_assert(!(n <= 0 || n > std::numeric_limits<U>::digits10));
    U denom = 0;
    for (int i = 0; i < n; i++) {
        denom *= 10;
        denom += 9;
    }
    return denom;
}

template<typename T>
constexpr std::enable_if_t<std::is_floating_point_v<T>, T> map(T value, T src_min, T src_max,
                                                               T dst_min, T dst_max) {
    return dst_min + ((value - src_min) * (dst_max - dst_min)) / (src_max - src_min);
}

template<typename T>
constexpr std::enable_if_t<std::is_floating_point_v<T>, T> normalize(T value, T src_min,
                                                                     T src_max) {
    T inv_scale = static_cast<T>(1.) / (src_max - src_min);
    return (value - src_min) * inv_scale;
}
/**
 * R is return type.
 * T is input type.
 * U is intermediate result type to avoid overflow/underflow.
 */
template<typename R, typename T, typename U = std::make_unsigned_t<T>>
constexpr std::enable_if_t<std::is_integral_v<T> && std::is_floating_point_v<R>, R>
normalize_integral(T value, T src_min, T src_max) {
    // Avoid the invalid case where src_min > src_max.
    if (src_min > src_max) [[unlikely]] {
        utils::trace("Invalid usage if tcode::normalize(", value, ", ", src_min, ", ", src_max,
                     ")!");
        return 0;
    }
    // Avoid divide-by-zero case.
    if (src_min == src_max) [[unlikely]] {
        return 0;
    }
    // Scale is always in range with the unsigned version of the input type range.
    U scale = static_cast<U>(src_max) - static_cast<U>(src_min);
    U centered = static_cast<U>(value) - static_cast<U>(src_min);
    return static_cast<R>(centered) / static_cast<R>(scale);
}

namespace hash {

// Use different constants for 32 bit vs. 64 bit size_t
constexpr std::size_t offset =
     std::conditional_t<sizeof(std::size_t) < 8, std::integral_constant<uint32_t, 0x811C9DC5>,
                        std::integral_constant<uint64_t, 0xCBF29CE484222325>>::value;
constexpr std::size_t prime =
     std::conditional_t<sizeof(std::size_t) < 8, std::integral_constant<uint32_t, 0x1000193>,
                        std::integral_constant<uint64_t, 0x100000001B3>>::value;

// FNV-1a hash
constexpr static std::size_t str(const char* s, const std::size_t value = offset) noexcept {
    return *s ? str(s + 1, (value ^ static_cast<std::size_t>(*s)) * prime) : value;
}

}  // namespace hash

}  // namespace tcode

#endif /*SEVFATE_TCODE_NUMERIC_HPP*/