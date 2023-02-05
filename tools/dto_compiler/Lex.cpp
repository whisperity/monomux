/* SPDX-License-Identifier: LGPL-3.0-only */
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <variant>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/SmallIndexMap.hpp"
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

/// Builds, contains, and executes a dynamically constructed character sequence
/// lexing automaton.
class CharSequenceLexer
{
  struct State
  {
    std::size_t Index;
  };

  struct AcceptState : public State
  {
    Token AcceptedToken;
  };

  struct ForwardState : public State
  {
    monomux::SmallIndexMap<std::size_t,
                           NumCharValues,
                           /* StoreInPlace =*/true,
                           /* IntrusiveDefaultSentinel =*/true,
                           /* KeyType =*/std::uint8_t>
      Next{};

#ifndef NDEBUG
    std::string DebugConsumedPrefix{};
#endif /* !NDEBUG */
  };

  struct ErrorState : public State
  {};

  using StateTy = std::variant<AcceptState, ForwardState, ErrorState>;

  std::vector<StateTy> States;

  monomux::SmallIndexMap<
    std::size_t,
    static_cast<std::size_t>(Token::LastTokenSentinel),
    /* StoreInPlace =*/true,
    /* IntrusiveDefaultSentinel =*/true,
    /* KeyType =*/std::make_unsigned_t<std::underlying_type_t<Token>>>
    Acceptors;

  std::size_t StartStateIndex = -1;

public:
  CharSequenceLexer()
  {
    // By default, error.
    States.emplace_back(ErrorState{0});
  }

  /// Lexes the given \p Chars into a \p Token based on the dynamic spelling
  /// table (created by \p addNewCharSequence)
  [[nodiscard]] std::optional<Token>
  lexToken(std::string_view Chars) const noexcept
  {
    std::size_t StateIndex = StartStateIndex;
    auto State = [&]() -> const StateTy& { return States.at(StateIndex); };

    std::string_view Buffer = Chars;
    while (true)
    {
      if (const auto* Forward = std::get_if<ForwardState>(&State()))
      {
        std::uint8_t NextChar = !Buffer.empty() ? Buffer.front() : '\0';
        if (const std::size_t* NextIndex = Forward->Next.tryGet(NextChar))
        {
          Buffer.remove_prefix(1);
          StateIndex = *NextIndex;
        }
        else
          return std::nullopt;
      }
      else if (const auto* Accept = std::get_if<AcceptState>(&State()))
        return Accept->AcceptedToken;
      else if (const auto* Error = std::get_if<ErrorState>(&State()))
        return std::nullopt;
      else
      {
        std::cerr << "ERROR: Unhandled state type at #" << StateIndex
                  << " when reading" << Buffer << " at the end of  " << Chars
                  << std::endl;
        return std::nullopt;
      }
    }
  }

  template <char... Cs> void addNewCharSequence(Token Tok)
  {
    const std::string Str = stringify<Cs...>();
    addNewCharSequence(Tok, Str);
  }

  void addNewCharSequence(Token Tok, std::string_view Str)
  {
    std::cerr << "CharSequence for Token::" << tokNiceName(Tok) << " = " << '"'
              << Str << '"' << " (size: " << Str.size() << ')' << std::endl;
    (void)createStartStateIfNone();
    States.reserve(States.size() + 1 /* Acceptor state */ +
                   Str.size() /* Letter transitions...*/ +
                   1 /* NULL transition */);

    const std::size_t AcceptStateIndex = [&]() {
      auto RawTok = static_cast<decltype(Acceptors)::key_type>(Tok);
      if (std::size_t* MaybeAcceptorIndex = Acceptors.tryGet(RawTok))
        return *MaybeAcceptorIndex;

      std::size_t I = makeState<AcceptState>(States.size(), Tok);
      Acceptors.set(RawTok, I);
      return I;
    }();
    std::cerr << '#' << AcceptStateIndex << " = AcceptState("
              << tokNiceName(Tok) << ')' << std::endl;

    continueBuildCharLexSequence(
      Str,
      std::get<AcceptState>(States.at(AcceptStateIndex)),
      std::get<ForwardState>(States.at(StartStateIndex)));
  }

private:
  template <typename T, typename... Args>
  [[nodiscard]] std::size_t makeState(Args&&... Argv)
  {
    return std::get<T>(States.emplace_back(T{{std::forward<Args>(Argv)}...}))
      .Index;
  }

  [[nodiscard]] std::size_t createStartStateIfNone()
  {
    if (StartStateIndex != static_cast<std::size_t>(-1))
      return StartStateIndex;
    std::cerr << '#' << States.size() << " = StartState" << std::endl;
    return StartStateIndex = makeState<ForwardState>(States.size());
  }

  void continueBuildCharLexSequence(std::string_view Str,
                                    const AcceptState& ExpectedAcceptState,
                                    ForwardState& ParentState)
  {
    if (Str.empty())
    {
      finishCharLexSequence(ExpectedAcceptState, ParentState);
      return;
    }

    const char C = Str.front();
    const std::size_t NextStateIndex = [&]() {
      if (std::size_t* MaybeNextIndex = ParentState.Next.tryGet(C))
        return *MaybeNextIndex;

      std::size_t NextStateIndex = makeState<ForwardState>(States.size());
#ifndef NDEBUG
      std::get<ForwardState>(States.at(NextStateIndex)).DebugConsumedPrefix =
        ParentState.DebugConsumedPrefix;
      std::get<ForwardState>(States.at(NextStateIndex))
        .DebugConsumedPrefix.push_back(C);
#endif /* !NDEBUG */

      return NextStateIndex;
    }();

    ParentState.Next.set(C, NextStateIndex);
    std::cerr << "step(#" << ParentState.Index << ", '" << C << "') := #"
              << NextStateIndex;
#ifndef NDEBUG
    std::cerr
      << " (" << '"'
      << std::get<ForwardState>(States.at(NextStateIndex)).DebugConsumedPrefix
      << '"' << ')';
#endif /* !NDEBUG */
    std::cerr << std::endl;


    if (Str.size() > 1)
      continueBuildCharLexSequence(
        Str.substr(1),
        ExpectedAcceptState,
        std::get<ForwardState>(States.at(NextStateIndex)));
    else
      // Add an empty transition with the NULL character to finish off the
      // automaton. This is needed as both "foo" and "foobar" might need to be
      // accepted by the client.
      continueBuildCharLexSequence(
        "\0",
        ExpectedAcceptState,
        std::get<ForwardState>(States.at(NextStateIndex)));
  }

  void finishCharLexSequence(const AcceptState& Acceptor,
                             ForwardState& ParentState)
  {
    auto PrintFinish = [&]() {
      std::cerr << "finish(#" << ParentState.Index;

#ifndef NDEBUG
      std::cerr << " /* " << '"' << ParentState.DebugConsumedPrefix << '"'
                << " */";
#endif /* !NDEBUG */

      std::cerr << ')';
    };

    if (std::size_t* NextIndex = ParentState.Next.tryGet('\0'))
    {
      std::cerr << "ERROR: Attempting to build non-deterministic automaton.\n";
      PrintFinish();
      std::cerr << " = #" << *NextIndex << " == Accept("
                << tokNiceName(
                     std::get<AcceptState>(States.at(*NextIndex)).AcceptedToken)
                << "), already.\nAttempted accepting "
                << tokNiceName(Acceptor.AcceptedToken) << " here instead."
                << std::endl;
      throw char{0};
    }

    ParentState.Next.set('\0', Acceptor.Index);
    PrintFinish();
    std::cerr << " := #" << Acceptor.Index << " == Accept("
              << tokNiceName(Acceptor.AcceptedToken) << ')' << std::endl;
  }
};

} // namespace detail

Lexer::~Lexer() = default;

Lexer::Lexer(std::string_view Buffer)
  : SeqLexer(std::make_unique<detail::CharSequenceLexer>()),
    OriginalFullBuffer(Buffer), CurrentState({Buffer, Token::NullToken, {}})
{
  setCurrentToken<Token::BeginningOfFile>();

  std::cerr << "DEBUG: Building sequenced lexical analysis table..."
            << std::endl;
#define STR_SPELLING_TOKEN(NAME, SPELLING)                                     \
  SeqLexer->addNewCharSequence(Token::NAME, SPELLING);
#include "Tokens.inc.h"
  std::cerr << "DEBUG: Lexical analysis table created." << std::endl;
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
        if (std::optional<Token> Keyword = SeqLexer->lexToken(TokenBuffer))
          return setCurrentTokenRaw(*Keyword, std::monostate{});

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
