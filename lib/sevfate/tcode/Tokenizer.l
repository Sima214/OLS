#include <tcode/Messages.hpp>
#include <tcode/ParserDispatcher.hpp>
#include <utils/Trace.hpp>
#include <utils/Z85.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace tcode {

namespace response {

void CommandIndex::parse(const char* begin) {
    cmd = common::char2cmdtyp(begin[0]);
    idx = begin[1] - '0';
    if (cmd == CommandType::Unknown) [[unlikely]] {
        utils::fatal("CommandIndex::parse: invalid cmd type");
    }
    if (idx > 9) [[unlikely]] {
        utils::fatal("CommandIndex::parse: idx out of bounds");
    }
}

void Z85Data::parse(const char* begin, const char* end) {
    size_t str_len = static_cast<uintptr_t>(end - begin);
    size_t code_count = str_len / 5;
    n = code_count * 4;

    memory.reset(new uint8_t[n]);
    if (!memory) {
        utils::fatal("Z85Data::parse: allocation failure");
    }
    data = memory.get();

    ::codec::decode(reinterpret_cast<uint32_t*>(memory.get()), code_count,
                    reinterpret_cast<const ::codec::z85_pack_t*>(begin), code_count);
}

static ErrorCode parse_error_code(const char* begin) {
    // Convert
    char ch = begin[1];
    uint8_t idx = ch - '0';
    // Check
    switch (static_cast<ErrorCode>(idx)) {
        case ErrorCode::Success:
        case ErrorCode::Tokenization:
        case ErrorCode::Parsing:
        case ErrorCode::Allocation:
        case ErrorCode::InvalidCommandIndex:
        case ErrorCode::UnknownProperty:
        case ErrorCode::InvalidOperation:
        case ErrorCode::Generic: {
            return static_cast<ErrorCode>(idx);
        }
        default: {
            // TODO: Crashes based on user-controlled value.
            utils::fatal("Invalid error code index");
        }
    }
}

}  // namespace response

#define parser_checked_parse(_parser_state, token, data)                                       \
    {                                                                                          \
        auto state = _parser_state.parse(token, data, *this);                                  \
        if (state == parser::yyParser::State::Continue) {                                      \
            continue;                                                                          \
        }                                                                                      \
        else [[unlikely]] {                                                                    \
            auto stream_idx = static_cast<uintptr_t>(pos - sequence_start);                    \
            _on_response_syntax_error(stream_idx, state);                                      \
            _parser_state.reset();                                                             \
            return false;                                                                      \
        }                                                                                      \
    }

bool ParserDispatcher::_tokenize(const char* const sequence_start, const size_t sequence_len) {
    const char* pos = sequence_start;
    const char* const end_pos = sequence_start + sequence_len;
    assert(*end_pos == '\0');
    assert(*(end_pos - 1) == '\n');
    const char* marked_pos;

    const char *t1, *t2;
    /*!stags:re2c format = 'const char *@@;\n'; */

    while (true) {
        /*!re2c
            re2c:define:YYCTYPE = char;
            re2c:define:YYCURSOR = pos;
            re2c:define:YYMARKER = marked_pos;
            re2c:define:YYLIMIT = end_pos;
            re2c:tags:prefix = tmp_tag_;
            re2c:yyfill:enable = 0;
            re2c:eof = 0;

            number = [0-9]+;
            z85 = ([\]\-0-9a-zA-Z/[*.:+=^!?&<>(){}@%$#]{5})+;

            command_axis = [LlRrVvAa][0-9];
            command = [LlRrVvAaDd][0-9];
            property_name = [0-9a-z_]+;

            *      {
                size_t idx = static_cast<uintptr_t>(pos - sequence_start);
                _on_response_tokenizer_error(idx);
                _parser_state.reset();
                return false;
            }
            $      {
                auto state = _parser_state.parse(parser::Token::Finalize, *this);
                if (state == parser::yyParser::State::Ok) {
                    return _on_response_end();
                }
                else [[unlikely]] {
                    auto stream_idx = static_cast<uintptr_t>(pos - sequence_start);
                    _on_response_syntax_error(stream_idx, state);
                    _parser_state.reset();
                    return false;
                }
            }
            '\n'   {
                parser_checked_parse(_parser_state, parser::Token::Exec, {});
                continue;
            }
            [ \t]+ {
                parser_checked_parse(_parser_state, parser::Token::Pad, {});
                continue;
            }
            @t1 command {
                response::CommandIndex cmd_idx;
                cmd_idx.parse(t1);
                parser_checked_parse(_parser_state, parser::Token::CmdIdx, cmd_idx);
                continue;
            }
            'P' @t1 property_name @t2 {
                auto prop = response::PropertyData(t1, t2);
                parser_checked_parse(_parser_state, parser::Token::Property, prop);
                continue;
            }
            'Z' @t1 z85 @t2 {
                response::Z85Data dat;
                dat.parse(t1, t2);
                parser_checked_parse(_parser_state, parser::Token::Z85Data, dat);
                continue;
            }
            @t1 [Ee][0-9] {
                auto err = response::parse_error_code(t1);
                parser_checked_parse(_parser_state, parser::Token::Error, err);
                continue;
            }
        */
    }
}

}  // namespace tcode
