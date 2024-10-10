#ifndef SEVFATE_FLAGS_CLASS_HPP
#define SEVFATE_FLAGS_CLASS_HPP
/**
 * @file
 * @brief Adapted from `vulkan.hpp` code.
 */

#include <type_traits>

namespace meta {

template<typename FlagBitsType> struct FlagTraits {
    static constexpr bool isBitmask = false;
};

template<typename BitType> class Flags {
   public:
    using MaskType = typename std::underlying_type<BitType>::type;

   private:
    MaskType _mask;

   public:
    // constructors
    constexpr Flags() noexcept : _mask(0) {}

    constexpr Flags(BitType bit) noexcept : _mask(static_cast<MaskType>(bit)) {}

    constexpr Flags(Flags<BitType> const& rhs) noexcept = default;

    constexpr explicit Flags(MaskType flags) noexcept : _mask(flags) {}

    // relational operators
    constexpr bool operator<(Flags<BitType> const& rhs) const noexcept {
        return _mask < rhs._mask;
    }

    constexpr bool operator<=(Flags<BitType> const& rhs) const noexcept {
        return _mask <= rhs._mask;
    }

    constexpr bool operator>(Flags<BitType> const& rhs) const noexcept {
        return _mask > rhs._mask;
    }

    constexpr bool operator>=(Flags<BitType> const& rhs) const noexcept {
        return _mask >= rhs._mask;
    }

    constexpr bool operator==(Flags<BitType> const& rhs) const noexcept {
        return _mask == rhs._mask;
    }

    constexpr bool operator!=(Flags<BitType> const& rhs) const noexcept {
        return _mask != rhs._mask;
    }

    // logical operator
    constexpr bool operator!() const noexcept {
        return !_mask;
    }

    // bitwise operators
    constexpr Flags<BitType> operator&(Flags<BitType> const& rhs) const noexcept {
        return Flags<BitType>(_mask & rhs._mask);
    }

    constexpr Flags<BitType> operator|(Flags<BitType> const& rhs) const noexcept {
        return Flags<BitType>(_mask | rhs._mask);
    }

    constexpr Flags<BitType> operator^(Flags<BitType> const& rhs) const noexcept {
        return Flags<BitType>(_mask ^ rhs._mask);
    }

    constexpr Flags<BitType> operator~() const noexcept {
        return Flags<BitType>(_mask ^ FlagTraits<BitType>::allFlags._mask);
    }

    // assignment operators
    constexpr Flags<BitType>& operator=(Flags<BitType> const& rhs) noexcept = default;

    constexpr Flags<BitType>& operator|=(Flags<BitType> const& rhs) noexcept {
        _mask |= rhs._mask;
        return *this;
    }

    constexpr Flags<BitType>& operator&=(Flags<BitType> const& rhs) noexcept {
        _mask &= rhs._mask;
        return *this;
    }

    constexpr Flags<BitType>& operator^=(Flags<BitType> const& rhs) noexcept {
        _mask ^= rhs._mask;
        return *this;
    }

    // cast operators
    explicit constexpr operator bool() const noexcept {
        return !!_mask;
    }

    explicit constexpr operator MaskType() const noexcept {
        return _mask;
    }
};

// relational operators only needed for pre C++20
template<typename BitType>
constexpr bool operator<(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator>(bit);
}

template<typename BitType>
constexpr bool operator<=(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator>=(bit);
}

template<typename BitType>
constexpr bool operator>(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator<(bit);
}

template<typename BitType>
constexpr bool operator>=(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator<=(bit);
}

template<typename BitType>
constexpr bool operator==(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator==(bit);
}

template<typename BitType>
constexpr bool operator!=(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator!=(bit);
}

// bitwise operators
template<typename BitType>
constexpr Flags<BitType> operator&(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator&(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator|(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator|(bit);
}

template<typename BitType>
constexpr Flags<BitType> operator^(BitType bit, Flags<BitType> const& flags) noexcept {
    return flags.operator^(bit);
}

// bitwise operators on BitType
template<typename BitType,
         typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator&(BitType lhs, BitType rhs) noexcept {
    return Flags<BitType>(lhs) & rhs;
}

template<typename BitType,
         typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept {
    return Flags<BitType>(lhs) | rhs;
}

template<typename BitType,
         typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator^(BitType lhs, BitType rhs) noexcept {
    return Flags<BitType>(lhs) ^ rhs;
}

template<typename BitType,
         typename std::enable_if<FlagTraits<BitType>::isBitmask, bool>::type = true>
inline constexpr Flags<BitType> operator~(BitType bit) noexcept {
    return ~(Flags<BitType>(bit));
}

}  // namespace meta

#endif /*SEVFATE_FLAGS_CLASS_HPP*/
