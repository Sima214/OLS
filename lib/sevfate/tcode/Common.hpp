#ifndef SEVFATE_TCODE_PARSER_COMMON_HPP
#define SEVFATE_TCODE_PARSER_COMMON_HPP
/**
 * @file
 * @brief Common macros, as well as forward declarations for ParserDispatcher.
 */

#ifdef MMCC_TCODE_BUILD
    #define MMCC_TCODE_API_EXPORT __attribute__((visibility("default")))
#else
    #define MMCC_TCODE_API_EXPORT
#endif

namespace tcode {
/* Fwd decls */
class ParserDispatcher;
}  // namespace tcode

#endif /*SEVFATE_TCODE_PARSER_COMMON_HPP*/