#ifndef SEVFATE_UTILS_CLASS_HPP
#define SEVFATE_UTILS_CLASS_HPP

#if __cplusplus >= 202002L
    #define sevf_meta_constexpr_since_cpp20 constexpr
    #define sevf_meta_consteval_since_cpp20 consteval
#else
    #define sevf_meta_constexpr_since_cpp20
    #define sevf_meta_consteval_since_cpp20 constexpr
#endif

#ifndef DEFINE_AUTO_PROPERTY
    /**
     * Automatically define setters and getters of Option/Factory classes.
     */
    #define DEFINE_AUTO_PROPERTY(name)                                                         \
        [[nodiscard]] auto name() const {                                                      \
            return _##name;                                                                    \
        }                                                                                      \
        auto name(decltype(_##name) name) {                                                    \
            _##name = name;                                                                    \
            return *this;                                                                      \
        }
#endif

#ifndef DEFINE_AUTO_VALIDATED_PROPERTY
    /**
     * Automatically define setters, getters and has_* of Option/Factory classes.
     */
    #define DEFINE_AUTO_VALIDATED_PROPERTY(name, invalid_value)                                \
        [[nodiscard]] auto name() const {                                                      \
            return _##name;                                                                    \
        }                                                                                      \
        bool has_##name() const {                                                              \
            return _##name != invalid_value;                                                   \
        }                                                                                      \
        auto name(decltype(_##name) name) {                                                    \
            _##name = name;                                                                    \
            return *this;                                                                      \
        }
#endif

namespace meta {

/**
 * @brief Inheritable class with disabled copy constructors.
 */
class INonCopyable {
   public:
    INonCopyable& operator=(const INonCopyable&) = delete;
    INonCopyable(const INonCopyable&) = delete;

    INonCopyable& operator=(INonCopyable&&) = default;
    INonCopyable(INonCopyable&&) = default;

    INonCopyable() = default;
    ~INonCopyable() = default;
};

}  // namespace meta

#endif /*SEVFATE_UTILS_CLASS_HPP*/