#ifndef UTILS_Z85_HPP
#define UTILS_Z85_HPP

#include <cstddef>
#include <cstdint>

namespace codec {

struct [[gnu::packed]] z85_pack_t {
    char str[5];
};
/** Sanity check packing. */
static_assert(sizeof(z85_pack_t[3]) == 3 * 5);
static_assert(sizeof(z85_pack_t[5]) == 5 * 5);

/**
 * Encodes the binary data in \p data to their z85 representation into \p string.
 *
 * \p string_n and \p data_n sizes represent element count.
 * @returns Encoded byte count. Always a multiple of 5.
 */
size_t encode(z85_pack_t* string, const size_t string_n, const uint32_t* data,
              const size_t data_n);

/**
 * Decodes the z85 sequence from \p string to the binary data in \p data.
 *
 * \p data_n and \p string_n sizes represent element count.
 * @returns Decoded byte count. Always a multiple of 4.
 */
size_t decode(uint32_t* data, const size_t data_n, const z85_pack_t* string,
              const size_t string_n);

}  // namespace codec

#endif /*UTILS_Z85_HPP*/