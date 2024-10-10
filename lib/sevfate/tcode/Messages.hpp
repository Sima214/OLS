#ifndef SEVFATE_TCODE_MESSAGES_HPP
#define SEVFATE_TCODE_MESSAGES_HPP

#include <tcode/Utils.hpp>
#include <utils/Class.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace tcode {

namespace common {

/** Supported types of command prefixes - axis and others combined. */
enum class CommandType : uint8_t {
    /* Meta */
    Unknown,
    /* Axis */
    Linear,
    Rotate,
    Vibrate,
    Auxiliary,
    /* Others */
    Device
};

inline constexpr CommandType char2cmdtyp(char c) {
    switch (c) {
        /* Axis */
        case 'L':
        case 'l': return CommandType::Linear;
        case 'R':
        case 'r': return CommandType::Rotate;
        case 'V':
        case 'v': return CommandType::Vibrate;
        case 'A':
        case 'a': return CommandType::Auxiliary;
        case 'D':
        case 'd': return CommandType::Device;
        default: return CommandType::Unknown;
    }
}

inline constexpr char cmdtyp2char(CommandType cmd) {
    switch (cmd) {
        case CommandType::Linear: return 'L';
        case CommandType::Rotate: return 'R';
        case CommandType::Vibrate: return 'V';
        case CommandType::Auxiliary: return 'A';
        case CommandType::Device: return 'D';
        case CommandType::Unknown:
        default: return '\0';
    }
}

struct CommandIndex {
    CommandType cmd = CommandType::Unknown;
    int8_t idx = -1;

    constexpr CommandIndex() = default;
    constexpr CommandIndex(CommandType cmd_, int8_t idx_) : cmd(cmd_), idx(idx_) {}

    constexpr operator uint16_t() const {
        return (static_cast<uint16_t>(cmd) << 8) | static_cast<uint16_t>(idx);
    }

    constexpr bool operator<(const CommandIndex& rhs) const {
        return static_cast<uint16_t>(*this) < static_cast<uint16_t>(rhs);
    }
    constexpr bool operator==(const CommandIndex& rhs) const {
        return static_cast<uint16_t>(*this) == static_cast<uint16_t>(rhs);
    }
    constexpr size_t hash() const {
        // TODO: Compare different methods, regarding performance and collisions.
        char cmd_char = cmdtyp2char(cmd);
        char idx_char = '0' + idx;
        auto value = tcode::hash::offset;
        constexpr auto prime = tcode::hash::prime;
        value = (value ^ static_cast<size_t>(cmd_char)) * prime;
        value = (value ^ static_cast<size_t>(idx_char)) * prime;
        return value;
    }

    std::array<char, 2> to_string() const {
        return {cmdtyp2char(cmd), static_cast<char>(idx + '0')};
    }

    std::string to_null_string() const {
        return {cmdtyp2char(cmd), static_cast<char>(idx + '0')};
    }
};

struct PropertyData {
    std::string_view name;

    constexpr PropertyData() = default;
    constexpr PropertyData(const char* begin, const char* end) :
        name(begin, static_cast<uintptr_t>(end - begin)) {}
    template<std::size_t N> constexpr PropertyData(const char (&str)[N]) {
        name = std::string_view(&str[0], N - 1);
    }
    constexpr PropertyData(std::string_view str) : name(str) {}
    sevf_meta_constexpr_since_cpp20 PropertyData(const std::string& str) : name(str) {}
};

}  // namespace common

namespace request {

using common::CommandType;

struct CommandIndex : public common::CommandIndex {
    constexpr CommandIndex() = default;
    constexpr CommandIndex(common::CommandIndex o) : common::CommandIndex{o.cmd, o.idx} {}
    constexpr CommandIndex(CommandType cmd_, int8_t idx_) : common::CommandIndex{cmd_, idx_} {
        if (cmd != CommandType::Linear && cmd != CommandType::Rotate &&
            cmd != CommandType::Vibrate && cmd != CommandType::Auxiliary &&
            cmd != CommandType::Device) {
            std::abort();
        }
        if (idx < 0 || idx > 9) {
            std::abort();
        }
    }
};
struct AxisUpdateData {
    CommandIndex cmd = {};
    fractional<uint32_t> value = {};

    constexpr AxisUpdateData() = default;
    constexpr AxisUpdateData(CommandIndex cmd_, fractional<uint32_t> value_) :
        cmd(cmd_), value(value_) {}
};

struct IntervalData {
    uint32_t interval = 0;

    constexpr IntervalData() = default;
    constexpr IntervalData(uint32_t interval_) : interval(interval_) {}
};
struct SpeedData {
    uint32_t speed = 0;

    constexpr SpeedData() = default;
    constexpr SpeedData(uint32_t speed_) : speed(speed_) {}
};

using common::PropertyData;
struct Z85Data {
    /** Decoded - binary data. */
    const uint8_t* data = nullptr;
    /** Size in bytes. */
    size_t n = 0;

    constexpr Z85Data() = default;
    constexpr Z85Data(const uint8_t* data_, size_t n_) {
        data = data_;
        n = n_;
    }
    constexpr Z85Data(const char* str_begin, const char* str_end) {
        /* NOTE: Do cast with void* intermediatery to avoid compilation errors. */
        data = static_cast<const uint8_t*>(static_cast<const void*>(str_begin));
        n = static_cast<uintptr_t>(str_end - str_begin);
    }
    template<std::size_t N> constexpr Z85Data(const char (&str)[N]) {
        data = static_cast<const uint8_t*>(static_cast<const void*>(str));
        n = static_cast<uintptr_t>(N - 1);
    }
    constexpr Z85Data(std::string_view str) {
        data = static_cast<const uint8_t*>(static_cast<const void*>(str.data()));
        n = str.size();
    }
};

}  // namespace request

namespace response {

using common::CommandType;

struct CommandIndex : public common::CommandIndex {
    constexpr CommandIndex() = default;

    void parse(const char* begin);
};

using common::PropertyData;

struct Z85Data : public request::Z85Data {
   protected:
    std::shared_ptr<uint8_t[]> memory;

   public:
    constexpr Z85Data() : request::Z85Data(), memory({}) {}

    void parse(const char* begin, const char* end);
};

enum class ErrorCode : uint8_t {
    Success = 0,
    Tokenization,
    Parsing,
    Allocation,
    InvalidCommandIndex,
    UnknownProperty,
    InvalidOperation,
    Generic = 9
};

struct Error {
    ErrorCode code = ErrorCode::Success;
    uint16_t extra_data = 0;
    uint16_t stream_idx = UINT16_MAX;
    std::string extra_msg = {};

    /** 0 byte payload */
    sevf_meta_constexpr_since_cpp20 Error() = default;
    sevf_meta_constexpr_since_cpp20 Error(ErrorCode code_) : code(code_) {}
    /** 4 byte payload */
    sevf_meta_constexpr_since_cpp20 Error(ErrorCode code_, uint16_t stream_idx_, uint16_t extra_data_ = 0) :
        code(code_), extra_data(extra_data_), stream_idx(stream_idx_) {}
    /** 4+n byte payload */
    sevf_meta_constexpr_since_cpp20 Error(ErrorCode code_, uint16_t stream_idx_, std::string&& extra_msg_) :
        code(code_), stream_idx(stream_idx_), extra_msg(std::move(extra_msg_)) {}
    sevf_meta_constexpr_since_cpp20 Error(ErrorCode code_, uint16_t stream_idx_, uint16_t extra_data_,
                    std::string&& extra_msg_) :
        code(code_),
        extra_data(extra_data_), stream_idx(stream_idx_), extra_msg(std::move(extra_msg_)) {}
    sevf_meta_constexpr_since_cpp20 Error(ErrorCode code_, std::string&& extra_msg_) :
        code(code_), stream_idx(UINT16_MAX), extra_msg(std::move(extra_msg_)) {}

    /** Return true if an error is set. */
    constexpr bool has_error() const {
        return code != ErrorCode::Success;
    };
    constexpr operator bool() const {
        return has_error();
    }

    static Error decode(ErrorCode code, response::Z85Data data);
};

using TokenData = std::variant<CommandIndex /*cmd_idx*/, PropertyData /*prop*/, Z85Data /*z85*/,
                               ErrorCode /*err*/>;

}  // namespace response

}  // namespace tcode

namespace std {

template<> struct hash<::tcode::common::CommandIndex> {
    constexpr size_t operator()(const ::tcode::common::CommandIndex cmd_idx) const {
        return static_cast<uint16_t>(cmd_idx);
    }
};

}  // namespace std

#endif /*SEVFATE_TCODE_MESSAGES_HPP*/