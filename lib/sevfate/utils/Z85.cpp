//  --------------------------------------------------------------------------
//  Reference implementation for rfc.zeromq.org/spec:32/Z85
//
//  This implementation provides a Z85 codec as an easy-to-reuse C class
//  designed to be easy to port into other languages.

//  --------------------------------------------------------------------------
//  Copyright (c) 2010-2013 iMatix Corporation and Contributors
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  --------------------------------------------------------------------------
#include "Z85.hpp"

#include <utils/Endianness.hpp>
#include <utils/Trace.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>

// Basic language taken from CZMQ's prelude
typedef uint8_t byte;
#define streq(s1, s2) (!std::strcmp((s1), (s2)))

namespace codec {

namespace {

//  Maps base 256 to base 85
static constexpr char ENCODER_LT[85] = {
     '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
     'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
     'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
     'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '.', '-', ':', '+', '=', '^',
     '!', '/', '*', '?', '&', '<', '>', '(', ')', '[', ']', '{', '}', '@', '%', '$', '#'};

//  Maps base 85 to base 256
//  We chop off lower 32 and higher 128 ranges
static constexpr byte DECODER_LT[96] = {
     0x00, 0x44, 0x00, 0x54, 0x53, 0x52, 0x48, 0x00, 0x4B, 0x4C, 0x46, 0x41, 0x00, 0x3F,
     0x3E, 0x45, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x40, 0x00,
     0x49, 0x42, 0x4A, 0x47, 0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
     0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
     0x3B, 0x3C, 0x3D, 0x4D, 0x00, 0x4E, 0x43, 0x00, 0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
     0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
     0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x4F, 0x00, 0x50, 0x00, 0x00};

}  // namespace

size_t encode(z85_pack_t* string, const size_t string_n, const uint32_t* data,
              const size_t data_n) {
    size_t out_pack_count = data_n;
    /* Verify that the output has enough space. */
    if (string_n < out_pack_count) {
        utils::fatal("codec::encode(z85): Not enough output space!");
    }
    size_t out_size = out_pack_count * 5;

    for (size_t i = 0; i < out_pack_count; i++) {
        uint32_t v = codec::hton(data[i]);
        for (uint32_t j = 0; j < 5; j++) {
            uint8_t lt_bd_idx = v % 85;
            string[i].str[4 - j] = ENCODER_LT[lt_bd_idx];
            v /= 85;
        }
    }

    return out_size;
}

size_t decode(uint32_t* data, const size_t data_n, const z85_pack_t* string,
              const size_t string_n) {
    size_t out_pack_count = string_n;
    /* Verify that the output has enough space. */
    if (data_n < out_pack_count) {
        utils::fatal("codec::decode(z85): Not enough output space!");
    }
    size_t out_size = out_pack_count * 4;

    for (size_t i = 0; i < out_pack_count; i++) {
        uint32_t v = 0;
        for (size_t j = 0; j < 5; j++) {
            v *= 85;
            size_t lt_idx = string[i].str[j] - 32;
            if (lt_idx < sizeof(DECODER_LT)) [[likely]] {
                v += DECODER_LT[lt_idx];
            }
        }
        data[i] = codec::ntoh(v);
    }

    return out_size;
}

}  // namespace codec
