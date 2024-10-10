/*
** 2000-05-29
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** Driver template for the LEMON parser generator.
**
** The "lemon" program processes an LALR(1) input grammar file, then uses
** this template to construct a parser.  The "lemon" program inserts text
** at each "%%" line.  Also, any "P-a-r-s-e" identifer prefix (without the
** interstitial "-" characters) contained in this template is changed into
** the value of the %name directive from the grammar.  Otherwise, the content
** of this template is copied straight through into the generate parser
** source file.
**
** The following is the concatenation of all %include directives from the
** input grammar file:
*/
/************ Begin %include sections from the grammar ************************/
%%
/**************** End of %include directives **********************************/

/* The next sections is a series of control #defines.
** various aspects of the generated parser.
**    YYNOCODE           is a number of type YYCODETYPE that is not used for
**                       any terminal or nonterminal symbol.
**    YYFALLBACK         If defined, this indicates that one or more tokens
**                       (also known as: "terminal symbols") have fall-back
**                       values which should be used if the original symbol
**                       would not parse.  This permits keywords to sometimes
**                       be used as identifiers, for example.
**    YYERRORSYMBOL      is the code number of the error symbol.  If not
**                       defined, then do no error processing.
**    YYNSTATE           the combined number of states.
**    YYNRULE            the number of rules in the grammar
**    YYNTOKEN           Number of terminal symbols
**    YY_MAX_SHIFT       Maximum value for shift actions
**    YY_MIN_SHIFTREDUCE Minimum value for shift-reduce actions
**    YY_MAX_SHIFTREDUCE Maximum value for shift-reduce actions
**    YY_ERROR_ACTION    The yy_action[] code for syntax error
**    YY_ACCEPT_ACTION   The yy_action[] code for accept
**    YY_NO_ACTION       The yy_action[] code for no-op
**    YY_MIN_REDUCE      Minimum value for reduce actions
**    YY_MAX_REDUCE      Maximum value for reduce actions
*/
/************* Begin control #defines *****************************************/
%%
/************* End control #defines *******************************************/
#define REDUCE_USER_ERROR YY_MAX_REDUCE
#define YY_NLOOKAHEAD ((int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])))

#include <cassert>

/* Define the yytestcase() macro to be a no-op if is not already defined
** otherwise.
**
** Applications can choose to define yytestcase() in the %include section
** to a macro that can assist in verifying code coverage.  For production
** code the yytestcase() macro should be turned off.  But it is useful
** for testing.
*/
#ifndef yytestcase
# define yytestcase(X)
#endif

#ifndef yyinvalidcase
# define yyinvalidcase(X, ruleno) assert(X)
#endif

namespace tcode::parser::impl {

/* Next are the tables used to determine what action to take based on the
** current state and lookahead token.  These tables are used to implement
** functions that take a state number and lookahead value and return an
** action integer.  
**
** Suppose the action integer is N.  Then the action is determined as
** follows
**
**   0 <= N <= YY_MAX_SHIFT             Shift N.  That is, push the lookahead
**                                      token onto the stack and goto state N.
**
**   N between YY_MIN_SHIFTREDUCE       Shift to an arbitrary state then
**     and YY_MAX_SHIFTREDUCE           reduce by rule N-YY_MIN_SHIFTREDUCE.
**
**   N == YY_ERROR_ACTION               A syntax error has occurred.
**
**   N == YY_ACCEPT_ACTION              The parser accepts its input.
**
**   N == YY_NO_ACTION                  No such action.  Denotes unused
**                                      slots in the yy_action[] table.
**
**   N between YY_MIN_REDUCE            Reduce by rule N-YY_MIN_REDUCE
**     and YY_MAX_REDUCE
**
** The action table is constructed as a single large table named yy_action[].
** Given state S and lookahead X, the action is computed as either:
**
**    (A)   N = yy_action[ yy_shift_ofst[S] + X ]
**    (B)   N = yy_default[S]
**
** The (A) formula is preferred.  The B formula is used instead if
** yy_lookahead[yy_shift_ofst[S]+X] is not equal to X.
**
** The formulas above are for computing the action when the lookahead is
** a terminal symbol.  If the lookahead is a non-terminal (as occurs after
** a reduce action) then the yy_reduce_ofst[] array is used in place of
** the yy_shift_ofst[] array.
**
** The following are the tables generated in this section:
**
**  yy_action[]        A single table containing all actions.
**  yy_lookahead[]     A table containing the lookahead for each entry in
**                     yy_action.  Used to detect hash collisions.
**  yy_shift_ofst[]    For each state, the offset into yy_action for
**                     shifting terminals.
**  yy_reduce_ofst[]   For each state, the offset into yy_action for
**                     shifting non-terminals after a reduce.
**  yy_default[]       Default action for each state.
**
*********** Begin parsing tables **********************************************/
%%
/********** End of lemon-generated parsing tables *****************************/

/* The next table maps tokens (terminal symbols) into fallback tokens.  
** If a construct like the following:
** 
**      %fallback ID X Y Z.
**
** appears in the grammar, then ID becomes a fallback token for X, Y,
** and Z.  Whenever one of the tokens X, Y, or Z is input to the parser
** but it does not parse, the type of the token is changed to ID and
** the parse is retried before an error is thrown.
**
** This feature can be used, for example, to cause some keywords in a language
** to revert to identifiers if they keyword does not apply in the context where
** it appears.
*/
#ifdef YYFALLBACK
static const YYCODETYPE yyFallback[] = {
%%
};
#endif /* YYFALLBACK */

/* For tracing shifts, the names of all terminals and nonterminals
** are required.  The following table supplies these names */
[[maybe_unused]] static const char* const yyTokenName[] = { 
%%
};

/* For tracing reduce actions, the names of all rules are required.
*/
[[maybe_unused]] static const char* const yyRuleName[] = {
%%
};

/* This array of booleans keeps track of the parser statement
** coverage.  The element yycoverage[X][Y] is set when the parser
** is in state X and has a lookahead token Y.  In a well-tested
** systems, every element of this matrix should end up being set.
*/
#if defined(YYCOVERAGE)
static bool yycoverage[YYNSTATE][YYNTOKEN];
#endif


/* The following function deletes the "minor type" or semantic value
** associated with a symbol.  The symbol can be either a terminal
** or nonterminal. "yymajor" is the symbol code, and "yypminor" is
** a pointer to the value to be deleted.  The code used to do the 
** deletions is derived from the %destructor and/or %token_destructor
** directives of the input grammar.
*/
static void yy_destructor(
  [[maybe_unused]] yyParser* yypParser,    /* The parser */
  YYCODETYPE yymajor,     /* Type code for object to destroy */
  [[maybe_unused]] YYMINORTYPE* yypminor   /* The object to be destroyed */
  ParseARG_PDECL          /* %extra_argument */
){
  ParseCTX_FETCH
  switch( yymajor ){
    /* Here is inserted the actions which take place when a
    ** terminal or non-terminal is destroyed.  This can happen
    ** when the symbol is popped from the stack during a
    ** reduce or during error processing or when a parser is 
    ** being destroyed before it is finished parsing.
    **
    ** Note: during a reduce, the only symbols destroyed are those
    ** which appear on the RHS of the rule, but which are* not* used
    ** inside the C code.
    */
/********* Begin destructor definitions ***************************************/
%%
/********* End destructor definitions *****************************************/
    default:  break;   /* If no destructor action specified: do nothing */
  }
}

/*
** Pop the parser's stack once.
**
** If there is a destructor routine associated with the token which
** is popped from the stack, then call it.
*/
static void yy_pop_parser_stack(yyParser* pParser ParseARG_PDECL){
  yyStackEntry* yytos;
  assert(pParser->yytos!=0);
  assert(pParser->yytos > pParser->yystack);
  yytos = pParser->yytos--;
  yy_destructor(pParser, yytos->major, &yytos->minor ParseARG_PARAM);
}

/*
** Find the appropriate action for a parser given the terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_shift_action(
  YYCODETYPE iLookAhead,    /* The look-ahead token */
  YYACTIONTYPE stateno      /* Current state number */
){
  int i;

  if( stateno>YY_MAX_SHIFT ) return stateno;
  assert(stateno <= YY_SHIFT_COUNT);
#if defined(YYCOVERAGE)
  yycoverage[stateno][iLookAhead] = 1;
#endif
  do{
    i = yy_shift_ofst[stateno];
    assert(i>=0);
    assert(i<=YY_ACTTAB_COUNT);
    assert(i+YYNTOKEN<=(int)YY_NLOOKAHEAD);
    assert(iLookAhead!=YYNOCODE);
    assert(iLookAhead < YYNTOKEN);
    i += iLookAhead;
    assert(i<(int)YY_NLOOKAHEAD);
    if( yy_lookahead[i]!=iLookAhead ){
#ifdef YYFALLBACK
      YYCODETYPE iFallback;            /* Fallback token */
      assert(iLookAhead<sizeof(yyFallback)/sizeof(yyFallback[0]));
      iFallback = yyFallback[iLookAhead];
      if( iFallback!=0 ){
        assert(yyFallback[iFallback]==0); /* Fallback loop must terminate */
        iLookAhead = iFallback;
        continue;
      }
#endif
#ifdef YYWILDCARD
      {
        int j = i - iLookAhead + YYWILDCARD;
        assert(j<(int)(sizeof(yy_lookahead)/sizeof(yy_lookahead[0])));
        if( yy_lookahead[j]==YYWILDCARD && iLookAhead>0 ){
          return yy_action[j];
        }
      }
#endif /* YYWILDCARD */
      return yy_default[stateno];
    }else{
      assert(i>=0 && i<(int)(sizeof(yy_action)/sizeof(yy_action[0])));
      return yy_action[i];
    }
  }while(1);
}

/*
** Find the appropriate action for a parser given the non-terminal
** look-ahead token iLookAhead.
*/
static YYACTIONTYPE yy_find_reduce_action(
  YYACTIONTYPE stateno,     /* Current state number */
  YYCODETYPE iLookAhead     /* The look-ahead token */
){
  int i;
#ifdef YYERRORSYMBOL
  if( stateno>YY_REDUCE_COUNT ){
    return yy_default[stateno];
  }
#else
  assert(stateno<=YY_REDUCE_COUNT);
#endif
  i = yy_reduce_ofst[stateno];
  assert(iLookAhead!=YYNOCODE);
  i += iLookAhead;
#ifdef YYERRORSYMBOL
  if( i<0 || i>=YY_ACTTAB_COUNT || yy_lookahead[i]!=iLookAhead ){
    return yy_default[stateno];
  }
#else
  assert(i>=0 && i<YY_ACTTAB_COUNT);
  assert(yy_lookahead[i]==iLookAhead);
#endif
  return yy_action[i];
}

/*
** The following routine is called if the stack overflows.
*/
static void yyStackOverflow(yyParser* yypParser ParseARG_PDECL){
   ParseCTX_FETCH
   while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser ParseARG_PARAM);
   /* Here code is inserted which will execute if the parser
   ** stack every overflows */
/******** Begin %stack_overflow code ******************************************/
%%
/******** End %stack_overflow code ********************************************/
}

/*
** Perform a shift action.
*/
static bool yy_shift(
  yyParser* yypParser,          /* The parser to be shifted */
  YYACTIONTYPE yyNewState,      /* The new state to shift in */
  YYCODETYPE yyMajor,           /* The major token to shift in */
  ParseTOKENTYPE yyMinor        /* The minor token to shift in */
  ParseARG_PDECL
){
  yyStackEntry* yytos;
  yypParser->yytos++;
#ifdef YYTRACKMAXSTACKDEPTH
  if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
    yypParser->yyhwm++;
    assert(yypParser->yyhwm == (int)(yypParser->yytos - yypParser->yystack));
  }
#endif
  if( yypParser->yytos>yypParser->yystackEnd ){
    yypParser->yytos--;
    yyStackOverflow(yypParser ParseARG_PARAM);
    return false;
  }
  if( yyNewState > YY_MAX_SHIFT ){
    yyNewState += YY_MIN_REDUCE - YY_MIN_SHIFTREDUCE;
  }
  yytos = yypParser->yytos;
  yytos->stateno = yyNewState;
  yytos->major = yyMajor;
  yytos->minor = yyMinor;
  return true;
}

/* For rule J, yyRuleInfoLhs[J] contains the symbol on the left-hand side
** of that rule */
static const YYCODETYPE yyRuleInfoLhs[] = {
%%
};

/* For rule J, yyRuleInfoNRhs[J] contains the negative of the number
** of symbols on the right-hand side of that rule. */
static const signed char yyRuleInfoNRhs[] = {
%%
};

static void yy_accept(yyParser* ParseARG_PDECL);  /* Forward Declaration */

/*
** Perform a reduce action and the shift that must immediately
** follow the reduce.
**
** The yyLookahead and yyLookaheadToken parameters provide reduce actions
** access to the lookahead token (if any).  The yyLookahead will be YYNOCODE
** if the lookahead token has already been consumed.  As this procedure is
** only called from one place, optimizing compilers will in-line it, which
** means that the extra parameters have no performance impact.
*/
static YYACTIONTYPE yy_reduce(
  yyParser* yypParser,         /* The parser */
  unsigned int yyruleno,       /* Number of the rule by which to reduce */
  int yyLookahead,             /* Lookahead token, or YYNOCODE if none */
  ParseTOKENTYPE yyLookaheadToken  /* Value of the lookahead token */
  ParseCTX_PDECL                   /* %extra_context */
  ParseARG_PDECL                   /* %extra_argument */
){
  int yygoto;                     /* The next state */
  YYACTIONTYPE yyact;             /* The next action */
  yyStackEntry* yymsp;            /* The top of the parser's stack */
  int yysize;                     /* Amount to pop the stack */
  (void)yyLookahead;
  (void)yyLookaheadToken;
  yymsp = yypParser->yytos;

  switch( yyruleno ){
  /* Beginning here are the reduction cases.  A typical example
  ** follows:
  **   case 0:
  **  #line <lineno> <grammarfile>
  **     { ... }           // User supplied code
  **  #line <lineno> <thisfile>
  **     break;
  */
/********** Begin reduce actions **********************************************/
%%
/********** End reduce actions ************************************************/
  };
  assert(yyruleno<sizeof(yyRuleInfoLhs)/sizeof(yyRuleInfoLhs[0]));
  yygoto = yyRuleInfoLhs[yyruleno];
  yysize = yyRuleInfoNRhs[yyruleno];
  yyact = yy_find_reduce_action(yymsp[yysize].stateno,(YYCODETYPE)yygoto);

  /* There are no SHIFTREDUCE actions on nonterminals because the table
  ** generator has simplified them to pure REDUCE actions. */
  assert(!(yyact>YY_MAX_SHIFT && yyact<=YY_MAX_SHIFTREDUCE));

  /* It is not possible for a REDUCE to be followed by an error */
  assert(yyact!=YY_ERROR_ACTION);

  yymsp += yysize+1;
  yypParser->yytos = yymsp;
  yymsp->stateno = (YYACTIONTYPE)yyact;
  yymsp->major = (YYCODETYPE)yygoto;
  return yyact;
}

/*
** The following code executes when the parse fails
*/
#ifndef YYNOERRORRECOVERY
static void yy_parse_failed(
  yyParser* yypParser      /* The parser */
  ParseARG_PDECL           /* %extra_argument */
){
  ParseCTX_FETCH
  while( yypParser->yytos>yypParser->yystack ) yy_pop_parser_stack(yypParser ParseARG_PARAM);
  /* Here code is inserted which will be executed whenever the
  ** parser fails */
/************ Begin %parse_failure code ***************************************/
%%
/************ End %parse_failure code *****************************************/
}
#endif /* YYNOERRORRECOVERY */

/*
** The following code executes when a syntax error first occurs.
*/
static void yy_syntax_error(
  [[maybe_unused]] yyParser* yypParser,           /* The parser */
  [[maybe_unused]] int yymajor,                   /* The major type of the error token */
  [[maybe_unused]] ParseTOKENTYPE yyminor         /* The minor type of the error token */
  ParseARG_PDECL                 /* %extra_argument */
){
  ParseCTX_FETCH
#define TOKEN yyminor
/************ Begin %syntax_error code ****************************************/
%%
/************ End %syntax_error code ******************************************/
}

/*
** The following is executed when the parser accepts
*/
static void yy_accept(
  [[maybe_unused]] yyParser* yypParser     /* The parser */
  ParseARG_PDECL          /* %extra_argument */
){
  ParseCTX_FETCH
#ifndef YYNOERRORRECOVERY
  yypParser->yyerrcnt = -1;
#endif
  assert(yypParser->yytos==yypParser->yystack);
  /* Here code is inserted which will be executed whenever the
  ** parser accepts */
/*********** Begin %parse_accept code *****************************************/
%%
/*********** End %parse_accept code *******************************************/
}

}

namespace tcode::parser {

/* Initialize a new parser that has already been allocated.
*/
void Parse_init(void* yypRawParser ParseCTX_PDECL){
  yyParser* yypParser = (yyParser*)yypRawParser;
  ParseCTX_STORE
  #ifdef YYTRACKMAXSTACKDEPTH
    yypParser->yyhwm = 0;
  #endif
  #ifndef YYNOERRORRECOVERY
    yypParser->yyerrcnt = -1;
  #endif
  yypParser->yytos = yypParser->yystack;
  yypParser->yystack[0].stateno = 0;
  yypParser->yystack[0].major = 0;
  yypParser->yystackEnd = &yypParser->yystack[YYSTACKDEPTH-1];
}

/*
** Clear all secondary memory allocations from the parser
*/
void Parse_finalize(void* p ParseARG_PDECL){
  yyParser* pParser = (yyParser*)p;
  while( pParser->yytos>pParser->yystack ) impl::yy_pop_parser_stack(pParser ParseARG_PARAM);
}

/*
** Return the peak depth of the stack for a parser.
*/
#ifdef YYTRACKMAXSTACKDEPTH
int Parse_stack_peak(void* p){
  yyParser* pParser = (yyParser*)p;
  return pParser->yyhwm;
}
#endif

/*
** Write into out a description of every state/lookahead combination that
**
**   (1)  has not been used by the parser, and
**   (2)  is not a syntax error.
**
** Return the number of missed state/lookahead combinations.
*/
#if defined(YYCOVERAGE)
int Parse_coverage(){
  int stateno, iLookAhead, i;
  int nMissed = 0;
  for(stateno=0; stateno<YYNSTATE; stateno++){
    i = impl::yy_shift_ofst[stateno];
    for(iLookAhead=0; iLookAhead<YYNTOKEN; iLookAhead++){
      if( impl::yy_lookahead[i+iLookAhead]!=iLookAhead ) continue;
      if( impl::yycoverage[stateno][iLookAhead]==0 ) nMissed++;
      printf("State %d lookahead %s %s\n", stateno,
              impl::yyTokenName[iLookAhead],
              impl::yycoverage[stateno][iLookAhead] ? "ok" : "missed");
    }
  }
  return nMissed;
}
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
yyParser::State Parse_parse(
  void* yyp,                   /* The parser */
  YYCODETYPE yymajor,          /* The major token code number */
  ParseTOKENTYPE&& yyminor       /* The value for the token */
  ParseARG_PDECL               /* Optional %extra_argument parameter */
){
  YYMINORTYPE yyminorunion;
  YYACTIONTYPE yyact;   /* The parser action. */
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  int yyendofinput;     /* True if we are at the end of input */
#endif
#ifdef YYERRORSYMBOL
  int yyerrorhit = 0;   /* True if yymajor has invoked an error */
#endif
  yyParser* yypParser = (yyParser*)yyp;  /* The parser */
  ParseCTX_FETCH

  assert(yypParser->yytos!=0);
#if !defined(YYERRORSYMBOL) && !defined(YYNOERRORRECOVERY)
  yyendofinput = (yymajor==0);
#endif

  yyact = yypParser->yytos->stateno;

  while(1){ /* Exit by break or return */
    assert(yypParser->yytos>=yypParser->yystack);
    assert(yyact==yypParser->yytos->stateno);
    yyact = impl::yy_find_shift_action((YYCODETYPE)yymajor,yyact);
    if( yyact >= YY_MIN_REDUCE ){
      unsigned int yyruleno = yyact - YY_MIN_REDUCE; /* Reduce by this rule */

      /* Check that the stack is large enough to grow by a single entry
      ** if the RHS of the rule is empty.  This ensures that there is room
      ** enough on the stack to push the LHS value */
      if( impl::yyRuleInfoNRhs[yyruleno]==0 ){
#ifdef YYTRACKMAXSTACKDEPTH
        if( (int)(yypParser->yytos - yypParser->yystack)>yypParser->yyhwm ){
          yypParser->yyhwm++;
          assert(yypParser->yyhwm ==
                  (int)(yypParser->yytos - yypParser->yystack), 25);
        }
#endif
#if YYSTACKDEPTH>0 
        if( yypParser->yytos>=yypParser->yystackEnd ){
          impl::yyStackOverflow(yypParser ParseARG_PARAM);
          return yyParser::State::StackOverflow; /* Invalid state! */
          break;
        }
#endif
      }
      yyact = impl::yy_reduce(yypParser,yyruleno,yymajor,yyminor ParseCTX_PARAM ParseARG_PARAM);
      if (yyact == REDUCE_USER_ERROR) {
        return yyParser::State::UserError;
      }
    }else if( yyact <= YY_MAX_SHIFTREDUCE ){
      if (!impl::yy_shift(yypParser, yyact,
                          (YYCODETYPE) yymajor, yyminor
                          ParseARG_PARAM)) {
        return yyParser::State::StackOverflow; /* Invalid state! */
      }
#ifndef YYNOERRORRECOVERY
      yypParser->yyerrcnt--;
#endif
      break;
    }else if( yyact==YY_ACCEPT_ACTION ){
      yypParser->yytos--;
      impl::yy_accept(yypParser ParseARG_PARAM);
      return yyParser::State::Ok;
    }else{
      assert(yyact == YY_ERROR_ACTION);
      yyminorunion = yyminor;
#ifdef YYERRORSYMBOL
      int yymx;
#endif
#ifdef YYERRORSYMBOL
      /* A syntax error has occurred.
      ** The response to an error depends upon whether or not the
      ** grammar defines an error token "ERROR".  
      **
      ** This is what we do if the grammar does define ERROR:
      **
      **  * Call the %syntax_error function.
      **
      **  * Begin popping the stack until we enter a state where
      **    it is legal to shift the error symbol, then shift
      **    the error symbol.
      **
      **  * Set the error count to three.
      **
      **  * Begin accepting and shifting new tokens.  No new error
      **    processing will occur until three tokens have been
      **    shifted successfully.
      **
      */
      if( yypParser->yyerrcnt<0 ){
        impl::yy_syntax_error(yypParser,yymajor,yyminor);
        return yyParser::State::SyntaxError; /* Invalid state! */
      }
      yymx = yypParser->yytos->major;
      if( yymx==YYERRORSYMBOL || yyerrorhit ){
        impl::yy_destructor(yypParser, (YYCODETYPE)yymajor, &yyminorunion);
        yymajor = YYNOCODE;
      }else{
        while( yypParser->yytos > yypParser->yystack ){
          yyact = impl::yy_find_reduce_action(yypParser->yytos->stateno,
                                        YYERRORSYMBOL);
          if( yyact<=YY_MAX_SHIFTREDUCE ) break;
          impl::yy_pop_parser_stack(yypParser ParseARG_PARAM);
        }
        if( yypParser->yytos <= yypParser->yystack || yymajor==0 ){
          impl::yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
          impl::yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
          yypParser->yyerrcnt = -1;
#endif
          yymajor = YYNOCODE;
          return yyParser::State::Failure; /* Invalid state! */
        }else if( yymx!=YYERRORSYMBOL ){
          if (!impl::yy_shift(yypParser,yyact,YYERRORSYMBOL,yyminor ParseARG_PARAM)) {
            return yyParser::State::StackOverflow; /* Invalid state! */
          }
        }
      }
      yypParser->yyerrcnt = 3;
      yyerrorhit = 1;
      if( yymajor==YYNOCODE ) break;
      yyact = yypParser->yytos->stateno;
#elif defined(YYNOERRORRECOVERY)
      /* If the YYNOERRORRECOVERY macro is defined, then do not attempt to
      ** do any kind of error recovery.  Instead, simply invoke the syntax
      ** error routine and continue going as if nothing had happened.
      **
      ** Applications can set this macro (for example inside %include) if
      ** they intend to abandon the parse upon the first syntax error seen.
      */
      impl::yy_syntax_error(yypParser,yymajor, yyminor ParseARG_PARAM);
      impl::yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion ParseARG_PARAM);
      return yyParser::State::SyntaxError; /* Invalid state! */
      break;
#else  /* YYERRORSYMBOL is not defined */
      /* This is what we do if the grammar does not define ERROR:
      **
      **  * Report an error message, and throw away the input token.
      **
      **  * If the input token is $, then fail the parse.
      **
      ** As before, subsequent error messages are suppressed until
      ** three input tokens have been successfully shifted.
      */
      if( yypParser->yyerrcnt<=0 ){
        impl::yy_syntax_error(yypParser,yymajor, yyminor);
        return yyParser::State::SyntaxError; /* Invalid state! */
      }
      yypParser->yyerrcnt = 3;
      impl::yy_destructor(yypParser,(YYCODETYPE)yymajor,&yyminorunion);
      if( yyendofinput ){
        impl::yy_parse_failed(yypParser);
#ifndef YYNOERRORRECOVERY
        yypParser->yyerrcnt = -1;
#endif
      }
      break;
#endif
    }
  }
  return yyParser::State::Continue;
}

/*
** Return the fallback token corresponding to canonical token iToken, or
** 0 if iToken has no fallback.
*/
int Parse_fallback(int iToken){
#ifdef YYFALLBACK
  assert(iToken<(int)(sizeof(yyFallback)/sizeof(yyFallback[0])));
  return impl::yyFallback[iToken];
#else
  (void)iToken;
  return 0;
#endif
}

}
