#ifndef SEVFATE_TCODE_PARSER_HPP
#define SEVFATE_TCODE_PARSER_HPP

#include <tcode/Messages.hpp>
#include <tcode/Common.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>

namespace tcode::parser {

/*
** User configuration overrides here.
*/
#define YYNOERRORRECOVERY
// #define YYTRACKMAXSTACKDEPTH
// #define YYCOVERAGE

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYCODETYPE         is the data type used to store the integer codes
**                       that represent terminal and non-terminal symbols.
**                       "unsigned char" is used if there are fewer than
**                       256 symbols.  Larger types otherwise.
**    YYACTIONTYPE       is the data type used for "action codes" - numbers
**                       that indicate what to do in response to the next
**                       token.
**    ParseTOKENTYPE     is the data type used for minor type for terminal
**                       symbols.  Background: A "minor type" is a semantic
**                       value associated with a terminal or non-terminal
**                       symbols.  For example, for an "ID" terminal symbol,
**                       the minor type might be the name of the identifier.
**                       Each non-terminal can have a different minor type.
**                       Terminal symbols all have the same minor type, though.
**                       This macros defines the minor type for terminal
**                       symbols.
**    YYMINORTYPE        is the data type used for all minor types.
**                       This is typically a union of many types, one of
**                       which is ParseTOKENTYPE.  The entry in the union
**                       for terminal symbols is called "yy0".
**    YYSTACKDEPTH       is the maximum depth of the parser's stack.  If
**                       zero the stack is dynamically sized using realloc()
**    ParseARG_SDECL     A static variable declaration for the %extra_argument
**    ParseARG_PDECL     A parameter declaration for the %extra_argument
**    ParseARG_PARAM     Code to pass %extra_argument as a subroutine parameter
**    ParseARG_STORE     Code to store %extra_argument into yypParser
**    ParseARG_FETCH     Code to extract %extra_argument from yypParser
**    ParseCTX_*         As ParseARG_ except for %extra_context
*/
/************* Begin #defines *****************************************/
%%
/************* End #defines *******************************************/

/* These constants specify the various numeric values for terminal symbols. */
enum class Token : YYCODETYPE {
    Finalize = 0,
    /***************** Begin token enum definitions *************************************/
%%
    /**************** End token enum definitions ***************************************/
};

/* The following structure represents a single element of the
** parser's stack.  Information stored includes:
**
**   +  The state number for the parser at this level of the stack.
**
**   +  The value of the token stored at this level of the stack.
**      (In other words, the "major" token.)
**
**   +  The semantic value stored at this level of the stack.  This is
**      the information used by the action routines in the grammar.
**      It is sometimes called the "minor" token.
**
** After the "shift" half of a SHIFTREDUCE action, the stateno field
** actually contains the reduce action for the second half of the
** SHIFTREDUCE.
*/
struct yyStackEntry {
    YYACTIONTYPE stateno = 0; /* The state-number, or reduce action in SHIFTREDUCE */
    YYCODETYPE major = 0;     /* The major token value.  This is the code
                               ** number for the token at this stack level */
    YYMINORTYPE minor = {};   /* The user-supplied minor token value.  This
                               ** is the value of the token  */
};

/* The state of the parser is completely contained in an instance of
** the following structure */
struct yyParser {
    /**
     * parse(...) return value.
     */
    enum class State {
        /** default return value */
        Continue,
        /** parse_accept */
        Ok,
        /** parse_failure */
        Failure,
        /** syntax_error */
        SyntaxError,
        /** stack_overflow */
        StackOverflow,
        /** User code has signaled an error */
        UserError
    };

    yyStackEntry* yytos = {};                /* Pointer to top element of the stack */
    yyStackEntry* yystackEnd = {};           /* Pointer to last entry in the stack */
    yyStackEntry yystack[YYSTACKDEPTH]; /* The parser's stack */
    #ifdef YYTRACKMAXSTACKDEPTH
        int yyhwm; /* High-water mark of the stack */
    #endif
    #ifndef YYNOERRORRECOVERY
        int yyerrcnt; /* Shifts left before out of the error */
    #endif
    #if YYSTACKDEPTH <= 0
        #error Parser stack on heap has been disabled.
    #endif
    ParseCTX_SDECL /* A place to hold %extra_context */

    /*** C++ style interface. ***/
    constexpr yyParser();

    /** Full parser state reset */
    // void reset(ParseCTX_PDECL);
    /** Reset parser state, while keeping the context variable. */
    constexpr void reset();

    size_t get_stack_usage() const;
    #ifdef YYTRACKMAXSTACKDEPTH
        size_t get_stack_peak_usage() const;
    #endif

    State parse(YYCODETYPE yymajor, ParseTOKENTYPE&& yyminor ParseARG_PDECL);
    State parse(Token yymajor, ParseTOKENTYPE&& yyminor ParseARG_PDECL);
    State parse(YYCODETYPE yymajor ParseARG_PDECL);
    State parse(Token yymajor ParseARG_PDECL);

    template<typename T> void finalize(T&& arg);
};

/* Initialize a new parser that has already been allocated.
 */
void Parse_init(void* yypRawParser ParseCTX_PDECL);

/*
** Clear all secondary memory allocations from the parser
*/
void Parse_finalize(void* p);

#ifdef YYTRACKMAXSTACKDEPTH
/*
** Return the peak depth of the stack for a parser.
*/
int Parse_stack_peak(void* p);
#endif

#if defined(YYCOVERAGE)
/*
** Write into out a description of every state/lookahead combination that
**
**   (1)  has not been used by the parser, and
**   (2)  is not a syntax error.
**
** Return the number of missed state/lookahead combinations.
*/
int Parse_coverage();
#endif

/* The main parser program.
** The first argument is a pointer to a structure obtained from
** "ParseAlloc" which describes the current state of the parser.
** The second argument is the major token number.  The third is
** the minor token.  The fourth optional argument is whatever the
** user wants (and specified in the grammar) and is available for
** use by the action routines.
**
** Inputs:
** <ul>
** <li> A pointer to the parser (an opaque structure.)
** <li> The major token number.
** <li> The minor token number.
** <li> An option argument of a grammar-specified type.
** </ul>
**
** Outputs:
** None.
*/
yyParser::State Parse_parse(void* yyp,             /* The parser */
                            YYCODETYPE yymajor,    /* The major token code number */
                            ParseTOKENTYPE&& yyminor /* The value for the token */
                                 ParseARG_PDECL    /* Optional %extra_argument parameter */
);

/*
** Return the fallback token corresponding to canonical token iToken, or
** 0 if iToken has no fallback.
*/
int Parse_fallback(int iToken);

inline constexpr yyParser::yyParser() {
    reset();
}
inline constexpr void yyParser::reset() {
    #ifdef YYTRACKMAXSTACKDEPTH
      yypParser->yyhwm = 0;
    #endif
    #ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt = -1;
    #endif
    yytos = &yystack[0];
    yystack[0].stateno = 0;
    yystack[0].major = 0;
    yystackEnd = &yystack[YYSTACKDEPTH-1];
}
inline size_t yyParser::get_stack_usage() const {
    return static_cast<uintptr_t>(yytos - yystack) / sizeof(yyStackEntry) + 1;
}
#ifdef YYTRACKMAXSTACKDEPTH
inline size_t yyParser::get_stack_peak_usage() const {
    return yyhwm;
}
#endif
inline yyParser::State yyParser::parse(YYCODETYPE yymajor,
                                       ParseTOKENTYPE&& yyminor ParseARG_PDECL) {
    return Parse_parse(this, yymajor, std::move(yyminor) ParseARG_PARAM);
}
inline yyParser::State yyParser::parse(Token yymajor, ParseTOKENTYPE&& yyminor ParseARG_PDECL) {
    return Parse_parse(this, static_cast<YYCODETYPE>(yymajor), std::move(yyminor) ParseARG_PARAM);
}
inline yyParser::State yyParser::parse(YYCODETYPE yymajor ParseARG_PDECL) {
    return Parse_parse(this, yymajor, {} ParseARG_PARAM);
}
inline yyParser::State yyParser::parse(Token yymajor ParseARG_PDECL) {
    return Parse_parse(this, static_cast<YYCODETYPE>(yymajor), {} ParseARG_PARAM);
}
template<typename T> inline void yyParser::finalize(T&& arg) {
    Parse_finalize(this, std::forward(arg));
}

}  // namespace tcode::parser

#endif /*SEVFATE_TCODE_PARSER_HPP*/