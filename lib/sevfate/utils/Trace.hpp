#ifndef SEVFATE_TRACE_HPP
#define SEVFATE_TRACE_HPP
/**
 * @file
 * @brief Isolated version of Trace.hpp from satr_spec.
 */

#include <cstdlib>
#include <iostream>

#if __cplusplus < 201703L
    #error C++17 support is required by this header!
#endif

namespace utils {

#ifndef NDEBUG

template<typename T> inline void trace(T msg) {
    std::cerr << msg << std::endl;
}

template<typename... Args> inline void trace(Args&&... args) {
    (std::cerr << ... << args) << std::endl;
}

#else

template<typename T> inline void trace([[maybe_unused]] T msg) {}
template<typename... Args> inline void trace([[maybe_unused]] Args&&... args) {}

#endif

template<typename T> inline __attribute__((__noreturn__)) void fatal(T msg) {
    trace<T>(msg);
    std::abort();
}

template<typename... Args> inline __attribute__((__noreturn__)) void fatal(Args&&... args) {
    trace(args...);
    std::abort();
}

}  // namespace utils

#endif /*SEVFATE_TRACE_HPP*/