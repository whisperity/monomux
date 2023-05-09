/* SPDX-License-Identifier: LGPL-3.0-only */
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/Metaprogramming.hpp"
#include "monomux/adt/SmallIndexMap.hpp"
#include "monomux/adt/StateMachine.hpp"
#include "monomux/unreachable.hpp"

#include "Lex.hpp"

namespace dto_compiler
{

std::string_view tokNiceName(Token TK)
{
  switch (TK)
  {
    default:
      return "Unknown TokenKind!";
#define TOKEN(NAME)                                                            \
  case Token::NAME:                                                            \
    return #NAME;
#include "Tokens.inc.h"
  };
}

#define BEFORE_TOKEN(NAME)                                                     \
  std::string TokenInfo<Token::NAME>::toString() const                         \
  {                                                                            \
    std::ostringstream OS;                                                     \
    OS << tokNiceName(Token::NAME) << '(';
#define TOKEN_INFO(TYPE, NAME)                                                 \
  OS << "/* " << #NAME << " =*/ ";                                             \
  if constexpr (std::is_same_v<TYPE, std::string> ||                           \
                std::is_same_v<TYPE, std::string_view>)                        \
  {                                                                            \
    OS << '"' << NAME << '"';                                                  \
  }                                                                            \
  else if constexpr (std::is_same_v<TYPE, bool>)                               \
  {                                                                            \
    OS << std::boolalpha << NAME << std::noboolalpha;                          \
  }                                                                            \
  else                                                                         \
  {                                                                            \
    OS << NAME;                                                                \
  }                                                                            \
  OS << ',' << ' ';
#define AFTER_TOKEN(NAME)                                                      \
  std::string S = OS.str();                                                    \
  if (S.back() == ' ')                                                         \
  {                                                                            \
    S = S.substr(0, S.size() - 2);                                             \
  }                                                                            \
  S.push_back(')');                                                            \
  return S;                                                                    \
  }
#include "Tokens.inc.h"

std::string tokToString(const AllTokenInfos& Info)
{
  /* NOLINTBEGIN(bugprone-macro-parantheses) */
#define TOKEN(NAME)                                                            \
  if (auto* NAME = std::get_if<TokenInfo<Token::NAME>>(&Info))                 \
    return NAME->toString();
#include "Tokens.inc.h"
  /* NOLINTEND(bugprone-macro-parantheses) */

  return "Unknown TokenKind!";
}

namespace detail
{

template <typename Str> void stringifyImpl(Str& /* S */)
{ /* Noop. */
}

template <typename Str, char C, char... Cs> void stringifyImpl(Str& S)
{
  S.push_back(C);
  stringifyImpl<Str, Cs...>(S);
}

template <char... Cs> std::string stringify()
{
  std::string S;
  stringifyImpl<std::string, Cs...>(S);
  return S;
}

constexpr std::size_t NumCharValues = (UINT8_MAX + 1);

// static const auto CommentLexer = []() {
//   using namespace monomux::state_machine;
//   auto Start = createMachine<char, void>();
//   auto Comment = Start.addOrTraverseTransition<'/'>();
//   auto LineCommentBegin = Comment.addOrTraverseTransition<'/'>();
//   auto BlockCommentBegin =
//   LineCommentBegin.switchToState<Comment.CurrentStateIndex>().addOrTraverseTransition<'*'>();
//
//   auto WithinComment = BlockCommentBegin.addNewState();
//   auto WithinCommentEdge =
//   WithinComment.switchToState<LineCommentBegin.CurrentStateIndex>().setDefaultTransitionTarget<WithinComment.CurrentStateIndex>();
//
//   auto WithinBlockComment =
//   WithinCommentEdge.switchToState<BlockCommentBegin.CurrentStateIndex>().addNewState();
//   auto WithinBlockCommentEdge =
//   WithinBlockComment.switchToState<BlockCommentBegin.CurrentStateIndex>().setDefaultTransitionTarget<WithinBlockComment.CurrentStateIndex>();
//
//   auto Machine = WithinBlockCommentEdge;
//   return compile(Machine);
// }();

template <class Machine, Token Tok, char... Str> struct TokenBuilder;
template <class Machine, Token Tok, char C, char... Cs>
struct TokenBuilder<Machine, Tok, C, Cs...>
{
  using type =
    typename TokenBuilder<decltype(
                            Machine{}.template addOrTraverseTransition<C>()),
                          Cs...>::type;
};
template <class Machine, Token Tok, char C> struct TokenBuilder<Machine, Tok, C>
{
  using type = typename decltype(
    Machine{}
      .template addOrTraverseTransition<C>()
      .template addOrTraverseTransition<'\0'>([]() {
        std::cout << "Accept " << tokNiceName(Tok) << std::endl;
      }))::type;
};

} // namespace detail

Lexer::~Lexer() = default;

Lexer::Lexer(std::string_view Buffer)
  : OriginalFullBuffer(Buffer), CurrentState({Buffer, Token::NullToken, {}})
{
  setCurrentToken<Token::BeginningOfFile>();

#define STR_SPELLING_TOKEN(NAME, SPELLING)                                     \
  /*SeqLexer->addNewCharSequence(Token::NAME, SPELLING)*/;
#include "Tokens.inc.h"
}

template <Token TK, typename... Args>
Token Lexer::setCurrentToken(Args&&... Argv)
{
  CurrentState.Info = TokenInfo<TK>{std::forward<Args>(Argv)...};
  return CurrentState.Tok = TK;
}

Token Lexer::setCurrentTokenRaw(Token TK, AllTokenInfos Info)
{
  switch (TK)
  {
#define TOKEN(NAME)                                                            \
  case Token::NAME:                                                            \
    if (std::holds_alternative<std::monostate>(Info))                          \
      Info = TokenInfo<Token::NAME>{};                                         \
    assert(std::holds_alternative<TokenInfo<Token::NAME>>(Info) &&             \
           "TokenKind " #NAME " mismatch contained value of Info struct");     \
    break;
#include "Tokens.inc.h"
    default:
      unreachable("Unknown TokenKind!");
  }

  CurrentState.Tok = TK;
  CurrentState.Info = Info;
  return TK;
}

Token Lexer::lexToken()
{
  std::string_view TokenBuffer = CurrentState.Buffer;
  auto TokenBufferEndAtReadBuffer = [&](std::size_t KeepCharsAtEnd = 0) {
    const std::size_t TokenLength =
      TokenBuffer.size() - CurrentState.Buffer.size() - KeepCharsAtEnd;
    if (KeepCharsAtEnd > 0)
    {
      // Save out the original buffer so KeepCharsAtEnd chars will effectively
      // be kept in the running read buffer.
      std::string_view OriginalBuffer = TokenBuffer;
      OriginalBuffer.remove_prefix(TokenLength);
      CurrentState.Buffer = OriginalBuffer;
    }
    TokenBuffer.remove_suffix(TokenBuffer.size() - TokenLength);
  };
  std::uint8_t Ch = getChar();

  switch (Ch)
  {
    case static_cast<std::uint8_t>(EOF):
      return setCurrentToken<Token::EndOfFile>();
    case ' ':
    case '\t':
    case '\n':
      return lexToken();
    case '\r':
      unreachable("getChar() should have never returned a '\r'");

    default:
      if (std::isalpha(Ch) || Ch == '_')
      {
        // Try lexing as an identifier...
        while (std::isalpha(Ch) || std::isdigit(Ch) || Ch == '_')
          Ch = getChar();

        TokenBufferEndAtReadBuffer(1); // Restore the last consumed char.

        // Check if identifier is reserved keyword.
        // if (std::optional<Token> Keyword = SeqLexer->lexToken(TokenBuffer))
        //   return setCurrentTokenRaw(*Keyword, std::monostate{});

        return setCurrentToken<Token::Identifier>(std::string{TokenBuffer});
      }

      TokenBufferEndAtReadBuffer();
      return setCurrentToken<Token::SyntaxError>(
        std::string("Unexpected ") + static_cast<char>(Ch) + " when reading " +
        std::string{TokenBuffer});

#define CH_SPELLING_TOKEN(NAME, SPELLING)                                      \
  case SPELLING:                                                               \
    return setCurrentToken<Token::NAME>();
#include "Tokens.inc.h"

    case '/':
    {
      // Consume various comment kinds.
      Ch = getChar();
      if (Ch == '/') // //
      {
        // "//" comments are line comments that should be stripped, unless
        // they are "//!" comments, which need to stay in the generated code.
        bool KeepComment = peekChar() == '!';
        if (KeepComment)
          (void)getChar();

        auto LineEndPos = CurrentState.Buffer.find_first_of("\r\n");
        if (LineEndPos == std::string_view::npos)
          LineEndPos = CurrentState.Buffer.size();

        // Discard until end of line.
        CurrentState.Buffer.remove_prefix(LineEndPos);
        if (KeepComment)
        {
          TokenBufferEndAtReadBuffer();
          TokenBuffer.remove_prefix(std::strlen("//!"));
          return setCurrentToken<Token::Comment>(false,
                                                 std::string{TokenBuffer});
        }
      }
      else if (Ch == '*') // /*
      {
        // "/*" comments are block comments that should be stripped, unless
        // the initial sequence is "/*!" which indicates it needs to stay.
        bool KeepComment = peekChar() == '!';
        if (KeepComment)
          (void)getChar();

        // Find where the comment block ends...
        [&]() -> void {
          std::size_t CommentDepth = 1;
          while (true)
          {
            Ch = getChar();
            switch (Ch)
            {
              case static_cast<std::uint8_t>(EOF):
                setCurrentToken<Token::SyntaxError>("Unterminated /* comment");
                return;
              case '/':
                if (peekChar() == '*')
                {
                  // Start of a new nested comment.
                  (void)getChar();
                  ++CommentDepth;
                }
                break;
              case '*':
                if (peekChar() == '/')
                {
                  // End of a nested comment.
                  (void)getChar();
                  --CommentDepth;
                }
                if (CommentDepth == 0)
                  return;
                break;
            }
          }
        }();

        if (KeepComment)
        {
          TokenBufferEndAtReadBuffer();
          TokenBuffer.remove_prefix(std::strlen("/*!"));
          TokenBuffer.remove_suffix(std::strlen("*/"));

          return setCurrentToken<Token::Comment>(true,
                                                 std::string{TokenBuffer});
        }
      }

      return lexToken();
    }
  }

  std::cerr << TokenBuffer << std::endl;
  unreachable("switch() statement should've returned appropriate Token");
}

Token Lexer::lex() { return lexToken(); }

Token Lexer::peek()
{
  State SavedState = CurrentState;
  Token Peeked = lexToken();
  CurrentState = SavedState;
  return Peeked;
}

std::uint8_t Lexer::getChar() noexcept
{
  if (CurrentState.Buffer.empty() ||
      (CurrentState.Buffer.size() == 1 && CurrentState.Buffer.front() == '\0'))
    return EOF;

  auto Ch = static_cast<std::uint8_t>(CurrentState.Buffer.front());
  // Ch has been consumed.
  CurrentState.Buffer.remove_prefix(1);

  switch (Ch)
  {
    default:
      return Ch;

    case '\0':
      // Observing a 0x00 (NUL) byte in the middle of the input stream means
      // something has gone wrong.
      std::cerr << "WARNING: Encountered NUL ('\\0') character at position "
                << (OriginalFullBuffer.size() - CurrentState.Buffer.size())
                << " before true EOF.\nReplacing with SPACE (' ')..."
                << std::endl;
      return ' ';

    case '\n':
    case '\r':
    {
      auto NextCh = static_cast<std::uint8_t>(CurrentState.Buffer.front());
      // Consume whitespace. If observing a "\n\r" or "\r\n" sequence, ignore
      // the '\r' and report it as a single newline.
      if ((NextCh == '\n' || NextCh == '\r') && NextCh != Ch)
        CurrentState.Buffer.remove_prefix(1);
      return '\n';
    }
  }
}

std::uint8_t Lexer::peekChar() noexcept
{
  auto BufferSave = CurrentState.Buffer;
  std::uint8_t Ch = getChar();
  CurrentState.Buffer = BufferSave;
  return Ch;
}

} // namespace dto_compiler
