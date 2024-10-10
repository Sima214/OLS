%name parser
%token_type { ::tcode::response::TokenData }
/* For each token parsed, also pass index to character sequence for error reporting. */
%extra_argument { ::tcode::ParserDispatcher& ctx }
/* Maximum exact stack usage for this grammar. */
%stack_size 6

%include {
#include <tcode/ParserDispatcher.hpp>
#include <utils/Trace.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
}

%parse_accept {}

%parse_failure {
    #error Disable this function by defining YYNOERRORRECOVERY.
}

%syntax_error {
    utils::trace("grammar parser: syntax error at token ", yyTokenName[yymajor]);
}

%stack_overflow {
    utils::fatal("grammar parser: stack overflow");
}

%start_symbol sequence

sequence ::= Exec.
sequence ::= codes Exec.

codes ::= codes Pad code.
codes ::= code.

code ::= CmdIdx(d) Z85Data(z). {
    if (!ctx._on_response(
        std::get<::tcode::response::CommandIndex>(d),
        std::get<::tcode::response::Z85Data>(z))
    ) [[unlikely]] {
        return REDUCE_USER_ERROR;
    }
}
code ::= CmdIdx(d) Property(p) Z85Data(z). {
    if (!ctx._on_response(
        std::get<::tcode::response::CommandIndex>(d),
        std::get<::tcode::response::PropertyData>(p),
        std::get<::tcode::response::Z85Data>(z))
    ) [[unlikely]] {
        return REDUCE_USER_ERROR;
    }
}

code ::= Error(e). {
    if (!ctx._on_response(
        std::get<::tcode::response::ErrorCode>(e))
    ) [[unlikely]] {
        return REDUCE_USER_ERROR;
    }
}
code ::= Error(e) Z85Data(z). {
    if (!ctx._on_response(
        std::get<::tcode::response::ErrorCode>(e),
        std::get<::tcode::response::Z85Data>(z))
    ) [[unlikely]] {
        return REDUCE_USER_ERROR;
    }
}
