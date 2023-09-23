/* SPDX-License-Identifier: LGPL-3.0-only */

/**
 * This file defines the "Token" kinds for the DTO DSL Compiler's lexical
 * analysis implementation.
 */

#ifndef TOKEN
#define TOKEN(NAME)
#endif /* TOKEN */

#ifndef BEFORE_TOKEN
#define BEFORE_TOKEN(NAME)
#endif /* BEFORE_TOKEN */

#ifndef AFTER_TOKEN
#define AFTER_TOKEN(NAME)
#endif /* AFTER_TOKEN */

#ifndef TOKEN_INFO
#define TOKEN_INFO(TYPE, NAME)
#endif /* TOKEN_INFO */

#ifndef SIMPLE_TOKEN
#define SIMPLE_TOKEN(NAME)                                                     \
  BEFORE_TOKEN(NAME)                                                           \
  TOKEN(NAME)                                                                  \
  AFTER_TOKEN(NAME)
#endif /* SIMPLE_TOKEN */

#ifndef CH_SPELLING_TOKEN
#define CH_SPELLING_TOKEN(NAME, SPELLING) SIMPLE_TOKEN(NAME)
#endif /* CH_SPELLING_TOKEN */

#ifndef SYMBOLIC_TOKEN
#define SYMBOLIC_TOKEN(NAME, SPELLING) SIMPLE_TOKEN(NAME)
#endif /* SYMBOL_TOKEN */

#ifndef STR_SPELLING_TOKEN
#define STR_SPELLING_TOKEN(NAME, ...) SIMPLE_TOKEN(NAME)
#endif /* STR_SPELLING_TOKEN */

#ifndef COMPLEX_TOKEN
#define COMPLEX_TOKEN(NAME, ARGS)                                              \
  BEFORE_TOKEN(NAME)                                                           \
  TOKEN(NAME)                                                                  \
  ARGS AFTER_TOKEN(NAME)
#endif /* COMPLEX_TOKEN */


/* Special markers ... */
SIMPLE_TOKEN(BeginningOfFile)
SIMPLE_TOKEN(EndOfFile)
COMPLEX_TOKEN(SyntaxError, TOKEN_INFO(std::string, Exception))
// SIMPLE_TOKEN(StripComment)

/* Verbatim structures ... */
COMPLEX_TOKEN(Identifier, TOKEN_INFO(std::string, Identifier))
COMPLEX_TOKEN(Integral, TOKEN_INFO(std::size_t, Value))
COMPLEX_TOKEN(Comment,
              TOKEN_INFO(bool, IsBlockComment) TOKEN_INFO(std::string, Comment))

/* Symbols ... */
CH_SPELLING_TOKEN(Comma, ',')
CH_SPELLING_TOKEN(Semicolon, ';')
CH_SPELLING_TOKEN(Eq, '=')
CH_SPELLING_TOKEN(LParen, '(')
CH_SPELLING_TOKEN(RParen, ')')
CH_SPELLING_TOKEN(LBrace, '{')
CH_SPELLING_TOKEN(RBrace, '}')
CH_SPELLING_TOKEN(LAcute, '<')
CH_SPELLING_TOKEN(RAcute, '>')

SYMBOLIC_TOKEN(Arrow, "->")
SYMBOLIC_TOKEN(Scope, "::")

/* Keywords ... */
STR_SPELLING_TOKEN(Namespace, "namespace")
STR_SPELLING_TOKEN(Literal, "literal")
STR_SPELLING_TOKEN(Function, "function")
STR_SPELLING_TOKEN(Record, "record")


#undef TOKEN
#undef BEFORE_TOKEN
#undef AFTER_TOKEN
#undef TOKEN_INFO
#undef SIMPLE_TOKEN
#undef CH_SPELLING_TOKEN
#undef SYMBOLIC_TOKEN
#undef STR_SPELLING_TOKEN
#undef COMPLEX_TOKEN
