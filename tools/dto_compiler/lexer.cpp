/* SPDX-License-Identifier: LGPL-3.0-only */
#include <algorithm>
#include <cassert>
#include <cstring>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>

#ifndef NDEBUG
#include <iostream>
#endif /* !NDEBUG */

#include "monomux/Debug.h"
#include "monomux/adt/SmallIndexMap.hpp"
#include "monomux/adt/scope_guard.hpp"
#include "monomux/unreachable.hpp"

#include "lexer.hpp"

namespace monomux::tools::dto_compiler
{

std::string_view to_string(token TK)
{
  switch (TK)
  {
    default:
      return "Unknown TokenKind!";
#define TOKEN(NAME)                                                            \
  case token::NAME:                                                            \
    return #NAME;
#include "Tokens.inc.h"
  };
}

#define BEFORE_TOKEN(NAME)                                                     \
  std::string token_info<token::NAME>::to_string() const                       \
  {                                                                            \
    std::ostringstream OS;                                                     \
    OS << dto_compiler::to_string(token::NAME) << '(';
#define TOKEN_INFO(TYPE, NAME)                                                 \
  OS << "/* " << #NAME << " =*/ ";                                             \
  if constexpr (std::is_same_v<TYPE, std::string> ||                           \
                std::is_same_v<TYPE, std::string_view>)                        \
  {                                                                            \
    OS << '"' << (NAME) << '"';                                                \
  }                                                                            \
  else if constexpr (std::is_same_v<TYPE, bool>)                               \
  {                                                                            \
    OS << std::boolalpha << (NAME) << std::noboolalpha;                        \
  }                                                                            \
  else                                                                         \
  {                                                                            \
    OS << (NAME);                                                              \
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

std::string to_string(const all_token_infos_type& Info)
{
  /* NOLINTBEGIN(bugprone-macro-parantheses) */
#define TOKEN(NAME)                                                            \
  if (const auto* NAME = std::get_if<token_info<token::NAME>>(&Info))          \
    return NAME->to_string();
#include "Tokens.inc.h"
  /* NOLINTEND(bugprone-macro-parantheses) */

  return "Unknown TokenKind!";
}

namespace detail
{

namespace
{

template <typename Str> void stringify_impl(Str& /* S */)
{ /* Noop. */
}

template <typename Str, char C, char... Cs> void stringify_impl(Str& S)
{
  S.push_back(C);
  stringify_impl<Str, Cs...>(S);
}

template <char... Cs> std::string stringify()
{
  std::string S;
  stringify_impl<std::string, Cs...>(S);
  return S;
}

} // namespace

constexpr std::size_t NumCharValues = (UINT8_MAX + 1);

/// Builds, contains, and executes a dynamically constructed character sequence
/// lexing automaton.
class char_sequence_lexer
{
  struct state
  {
    std::size_t Index;
  };

  struct accept_state : public state
  {
    token AcceptedToken;
  };

  struct forward_state : public state
  {
    monomux::SmallIndexMap<std::size_t,
                           NumCharValues,
                           /* StoreInPlace =*/true,
                           /* IntrusiveDefaultSentinel =*/true,
                           /* KeyType =*/lexer::Char>
      Next{};

    /// If set and a transition in \p Next is not set, instead of going into
    /// the error state, transition to the state specified by the \e index
    /// here.
    std::optional<std::size_t> DefaultNextState;

    [[nodiscard]] std::size_t get_next_state(lexer::Char TransitionChar) const
    {
      if (const std::size_t* NextIndex = Next.tryGet(TransitionChar))
        return *NextIndex;
      if (DefaultNextState)
        return *DefaultNextState;
      return -1;
    }

#ifndef NDEBUG
    std::string DebugConsumedPrefix{};
#endif /* !NDEBUG */
  };

  struct error_state : public state
  {};

  using states = std::variant<accept_state, forward_state, error_state>;
  std::vector<states> States;

  monomux::SmallIndexMap<
    std::size_t,
    static_cast<std::size_t>(token::LastTokenSentinel),
    /* StoreInPlace =*/true,
    /* IntrusiveDefaultSentinel =*/true,
    /* KeyType =*/std::make_unsigned_t<std::underlying_type_t<token>>>
    Acceptors;

  std::size_t StartStateIndex = -1;

public:
  char_sequence_lexer()
  {
    // By default, error.
    States.emplace_back(error_state{0});
  }

  /// Lexes the given \p Chars into a \p Token based on the dynamic spelling
  /// table (created by \p addNewCharSequence)
  [[nodiscard]] std::optional<token>
  lex_token(std::string_view Chars) const noexcept
  {
    std::size_t StateIndex = StartStateIndex;
    auto State = [&]() -> const states& { return States.at(StateIndex); };

    std::string_view Buffer = Chars;
    while (true)
    {
      if (const auto* Forward = std::get_if<forward_state>(&State()))
      {
        const lexer::Char NextChar = !Buffer.empty() ? Buffer.front() : '\0';
        const std::size_t NextIndex = Forward->get_next_state(NextChar);
        if (NextIndex == static_cast<std::size_t>(-1))
          // Received error state.
          return std::nullopt;

        Buffer.remove_prefix(1);
        StateIndex = NextIndex;
      }
      else if (const auto* Accept = std::get_if<accept_state>(&State()))
        return Accept->AcceptedToken;
      else if (std::holds_alternative<error_state>(State()))
        return std::nullopt;
      else
      {
        MONOMUX_DEBUG(std::cerr << "ERROR: Unhandled state type at #"
                                << StateIndex << " when reading" << Buffer
                                << " at the end of  " << Chars << std::endl);
        return std::nullopt;
      }
    }
  }

  template <char... Cs> void add_new_char_sequence(token Tok)
  {
    const std::string Str = stringify<Cs...>();
    add_new_char_sequence(Tok, Str);
  }

  void add_new_char_sequence(token Tok, std::string_view Str)
  {
    MONOMUX_DEBUG(std::cerr << "CharSequence for Token::" << to_string(Tok)
                            << " = " << '"' << Str << '"'
                            << " (size: " << Str.size() << ')' << std::endl);
    (void)create_start_state_if_none();
    States.reserve(States.size() + 1 /* Acceptor state */ +
                   Str.size() /* Letter transitions...*/ +
                   1 /* NULL transition */);

    const std::size_t AcceptStateIndex = [&]() {
      auto RawTok = static_cast<decltype(Acceptors)::key_type>(Tok);
      if (std::size_t* MaybeAcceptorIndex = Acceptors.tryGet(RawTok))
        return *MaybeAcceptorIndex;

      std::size_t I = make_state<accept_state>(States.size(), Tok);
      Acceptors.set(RawTok, I);
      return I;
    }();
    MONOMUX_DEBUG(std::cerr << '#' << AcceptStateIndex << " = AcceptState("
                            << to_string(Tok) << ')' << std::endl);

    continue_building_char_lex_sequence(
      Str,
      std::get<accept_state>(States.at(AcceptStateIndex)),
      std::get<forward_state>(States.at(StartStateIndex)));
  }

private:
  template <typename T, typename... Args>
  [[nodiscard]] std::size_t make_state(Args&&... Argv)
  {
    // NOLINT(-Wmissing-...): Using an additional set of {}s breaks G++-8
    // on Ubuntu 18.04.
    return std::get<T>(States.emplace_back(T{std::forward<Args>(Argv)...}))
      .Index;
  }

  [[nodiscard]] std::size_t create_start_state_if_none()
  {
    if (StartStateIndex != static_cast<std::size_t>(-1))
      return StartStateIndex;
    MONOMUX_DEBUG(std::cerr << '#' << States.size() << " = StartState"
                            << std::endl);
    return StartStateIndex = make_state<forward_state>(States.size());
  }

  void
  continue_building_char_lex_sequence(std::string_view Str,
                                      const accept_state& ExpectedAcceptState,
                                      forward_state& ParentState)
  {
    if (Str.empty())
    {
      finish_char_lex_sequence(ExpectedAcceptState, ParentState);
      return;
    }

    const char C = Str.front();
    const std::size_t NextStateIndex = [&]() {
      if (std::size_t* MaybeNextIndex = ParentState.Next.tryGet(C))
        return *MaybeNextIndex;

      std::size_t NextStateIndex = make_state<forward_state>(States.size());
      MONOMUX_DEBUG(
        std::get<forward_state>(States.at(NextStateIndex)).DebugConsumedPrefix =
          ParentState.DebugConsumedPrefix;
        std::get<forward_state>(States.at(NextStateIndex))
          .DebugConsumedPrefix.push_back(C););
      return NextStateIndex;
    }();

    ParentState.Next.set(C, NextStateIndex);
    MONOMUX_DEBUG(
      std::cerr << "step(#" << ParentState.Index << ", '" << C << "') := #"
                << NextStateIndex;
      std::cerr
      << " (" << '"'
      << std::get<forward_state>(States.at(NextStateIndex)).DebugConsumedPrefix
      << '"' << ')';
      std::cerr << std::endl;);

    if (Str.size() > 1)
      continue_building_char_lex_sequence(
        Str.substr(1),
        ExpectedAcceptState,
        std::get<forward_state>(States.at(NextStateIndex)));
    else
      // Add an empty transition with the NULL character to finish off the
      // automaton. This is needed as both "foo" and "foobar" might need to be
      // accepted by the client.
      continue_building_char_lex_sequence(
        "\0",
        ExpectedAcceptState,
        std::get<forward_state>(States.at(NextStateIndex)));
  }

  void finish_char_lex_sequence(const accept_state& Acceptor,
                                forward_state& ParentState)
  {
#ifndef NDEBUG
    auto PrintFinish = [&]() {
      std::cerr << "finish(#" << ParentState.Index;

      MONOMUX_DEBUG(std::cerr << " /* " << '"'
                              << ParentState.DebugConsumedPrefix << '"'
                              << " */");

      std::cerr << ')';
    };
#endif /* !NDEBUG */

    if (const std::size_t* NextIndex = ParentState.Next.tryGet('\0'))
    {
      MONOMUX_DEBUG(
        std::cerr
          << "ERROR: Attempting to build non-deterministic automaton.\n";
        PrintFinish();
        std::cerr
        << " = #" << *NextIndex << " == Accept("
        << to_string(
             std::get<accept_state>(States.at(*NextIndex)).AcceptedToken)
        << "), already.\nAttempted accepting "
        << to_string(Acceptor.AcceptedToken) << " here instead." << std::endl;);
      throw char{0};
    }

    ParentState.Next.set('\0', Acceptor.Index);
    MONOMUX_DEBUG(PrintFinish(); std::cerr
                                 << " := #" << Acceptor.Index << " == Accept("
                                 << to_string(Acceptor.AcceptedToken) << ')'
                                 << std::endl;);
  }
};

} // namespace detail

lexer::location lexer::location::make_location(std::string_view FullBuffer,
                                               std::string_view Buffer) noexcept
{
  assert(FullBuffer.size() >= Buffer.size() && "Buffer overflow!");
  const std::size_t LocationFromStart = FullBuffer.size() - Buffer.size();
  return make_location(FullBuffer, LocationFromStart);
}

lexer::location lexer::location::make_location(std::string_view Buffer,
                                               std::size_t AbsoluteLoc) noexcept
{
  std::string_view BufferBeforeLoc = Buffer;
  BufferBeforeLoc.remove_suffix(BufferBeforeLoc.size() - AbsoluteLoc);

  const std::size_t Rows = std::count_if(BufferBeforeLoc.begin(),
                                         BufferBeforeLoc.end(),
                                         [](char Ch) { return Ch == '\n'; });
  const decltype(std::string_view::npos) LastNewlineIndex =
    BufferBeforeLoc.find_last_of('\n');
  return location{.Absolute = AbsoluteLoc,
                  .Line = Rows + 1,
                  .Column = BufferBeforeLoc.size() - LastNewlineIndex};
}

lexer::~lexer() = default;

lexer::lexer(std::string_view Buffer)
  : SequenceLexer(std::make_unique<detail::char_sequence_lexer>()),
    OriginalFullBuffer(Buffer), CurrentState({Buffer, token::NullToken, {}, {}})
{
  set_current_token<token::BeginningOfFile>();

  MONOMUX_DEBUG(
    std::cerr << "DEBUG: Building sequenced lexical analysis table..."
              << std::endl);
#define STR_SPELLING_TOKEN(NAME, SPELLING)                                     \
  SequenceLexer->add_new_char_sequence(token::NAME, SPELLING);
#include "Tokens.inc.h"
  MONOMUX_DEBUG(std::cerr << "DEBUG: Lexical analysis table created."
                          << std::endl);
}

template <token TK, typename... Args>
token lexer::set_current_token(Args&&... Argv) noexcept
{
  CurrentState.Info = token_info<TK>{std::forward<Args>(Argv)...};
  return CurrentState.Tok = TK;
}

token lexer::set_current_token_raw(token TK, all_token_infos_type Info) noexcept
{
  switch (TK)
  {
#define TOKEN(NAME)                                                            \
  case token::NAME:                                                            \
    if (std::holds_alternative<std::monostate>(Info))                          \
      Info = token_info<token::NAME>{};                                        \
    assert(std::holds_alternative<token_info<token::NAME>>(Info) &&            \
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

void lexer::token_buffer_set_end_at_consumed_buffer(
  std::string_view& TokenBuffer, std::size_t KeepCharsAtEnd) noexcept
{
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
}

token lexer::lex_token() noexcept
{
  std::string_view TokenBuffer = CurrentState.Buffer;
  CurrentState.Loc = OriginalFullBuffer.size() - CurrentState.Buffer.size();
  auto TokenBufferEndAtReadBuffer = [&](std::size_t KeepCharsAtEnd = 0) {
    token_buffer_set_end_at_consumed_buffer(TokenBuffer, KeepCharsAtEnd);
  };
  Char Ch = get_char();

  switch (Ch)
  {
    case static_cast<Char>(EOF):
      return set_current_token<token::EndOfFile>();
    case ' ':
    case '\t':
    case '\n':
      return lex_token();
    case '\r':
      unreachable("getChar() should have never returned a '\r'!");

#define CH_SPELLING_TOKEN(NAME, SPELLING)                                      \
  case SPELLING:                                                               \
    return set_current_token<token::NAME>();
#include "Tokens.inc.h"

    case '/':
    {
      // Consume various comment kinds.
      Ch = get_char();
      token T{};
      if (Ch == '/') // //
        T = lex_comment(TokenBuffer, /*MultiLine=*/false);
      else if (Ch == '*') // /*
        T = lex_comment(TokenBuffer, /*MultiLine=*/true);

      if (T == token::Comment)
        return T;
      if (T == token::NullToken)
        // The comment did not need to be lexed, as it will be stripped from the
        // output.
        return lex_token();

      return set_current_token<token::SyntaxError>(
        std::string("Unexpected ") + static_cast<char>(Ch) + " when reading " +
        std::string{TokenBuffer});
    }

    case ':':
    {
      Ch = get_char();
      if (Ch == ':') // ::
        return set_current_token<token::Scope>();

      return set_current_token<token::SyntaxError>(
        std::string("Unexpected ") + static_cast<char>(Ch) + " when reading " +
        std::string{TokenBuffer});
    }

    case '-':
    {
      if (peek_char() == '>') // ->
      {
        (void)get_char(); // Consume the '>'.
        return set_current_token<token::Arrow>();
      }

      return lex_integer_literal(TokenBuffer);
    }

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return lex_integer_literal(TokenBuffer);

    default:
    {
      if (std::isalpha(Ch) || Ch == '_')
      {
        // Try lexing as an identifier...
        while (std::isalpha(Ch) || std::isdigit(Ch) || Ch == '_')
          Ch = get_char();
        TokenBufferEndAtReadBuffer(1); // Restore the last consumed char.

        // Check if identifier is reserved keyword.
        if (const std::optional<token> Keyword =
              SequenceLexer->lex_token(TokenBuffer))
          return set_current_token_raw(*Keyword, std::monostate{});

        return set_current_token<token::Identifier>(std::string{TokenBuffer});
      }

      TokenBufferEndAtReadBuffer();
      return set_current_token<token::SyntaxError>(
        std::string("Unexpected ") + static_cast<char>(Ch) + " when reading " +
        std::string{TokenBuffer});
    }
  }

  MONOMUX_DEBUG(std::cerr << TokenBuffer << std::endl);
  unreachable("switch() statement should've returned appropriate Token");
}

token lexer::lex_comment(std::string_view& TokenBuffer, bool MultiLine) noexcept
{
  // "//" comments are line comments that should be stripped, unless
  // they are "//!" comments, which need to stay in the generated code.
  //
  // "/*" comments are block comments that should be stripped, unless
  // the initial sequence is "/*!" which indicates it needs to stay.
  const bool KeepComment = peek_char() == '!';
  if (KeepComment)
    (void)get_char(); // Consume the '!'.

  if (!MultiLine)
  {
    auto LineEndPos = CurrentState.Buffer.find_first_of("\r\n");
    if (LineEndPos == std::string_view::npos)
      LineEndPos = CurrentState.Buffer.size();

    // Discard until end of line.
    CurrentState.Buffer.remove_prefix(LineEndPos);
    if (KeepComment)
    {
      token_buffer_set_end_at_consumed_buffer(TokenBuffer);
      TokenBuffer.remove_prefix(std::strlen("//!"));
      return set_current_token<token::Comment>(false, std::string{TokenBuffer});
    }
  }
  else
  {
    // Find where the comment block ends...
    [&]() -> void {
      std::size_t CommentDepth = 1;
      while (true)
      {
        switch (get_char())
        {
          case static_cast<Char>(EOF):
            set_current_token<token::SyntaxError>("Unterminated /* comment");
            return;
          case '/':
            if (peek_char() == '*')
            {
              // Start of a new nested comment.
              (void)get_char();
              ++CommentDepth;
            }
            break;
          case '*':
            if (peek_char() == '/')
            {
              // End of a nested comment.
              (void)get_char();
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
      token_buffer_set_end_at_consumed_buffer(TokenBuffer);
      TokenBuffer.remove_prefix(std::strlen("/*!"));
      TokenBuffer.remove_suffix(std::strlen("*/"));

      return set_current_token<token::Comment>(true, std::string{TokenBuffer});
    }
  }

  return token::NullToken;
}

token lexer::lex_integer_literal(std::string_view& TokenBuffer) noexcept
{
  bool IsNegative = TokenBuffer.front() == '-';
  Char Ch = get_char();
  while (std::isdigit(Ch))
    Ch = get_char();

  // Restore the last consumed non-digit.
  token_buffer_set_end_at_consumed_buffer(TokenBuffer, 1);

  const std::string TokenValueStr{TokenBuffer};
  return set_current_token<token::Integral>(
    IsNegative ? std::stoll(TokenValueStr) : std::stoull(TokenValueStr));
}


token lexer::lex() noexcept { return lex_token(); }

token lexer::peek() noexcept
{
  restore_guard G{CurrentState};
  return lex_token();
}

lexer::location lexer::get_location() const noexcept
{
  return location::make_location(OriginalFullBuffer, CurrentState.Loc);
}

lexer::Char lexer::get_char() noexcept
{
  if (CurrentState.Buffer.empty() ||
      (CurrentState.Buffer.size() == 1 && CurrentState.Buffer.front() == '\0'))
    return EOF;

  auto Ch = static_cast<Char>(CurrentState.Buffer.front());
  // Ch has been consumed.
  CurrentState.Buffer.remove_prefix(1);

  switch (Ch)
  {
    default:
      return Ch;

    case '\0':
      // Observing a 0x00 (NUL) byte in the middle of the input stream means
      // something has gone wrong.
      MONOMUX_DEBUG(std::cerr
                    << "WARNING: Encountered NUL ('\\0') character at position "
                    << (OriginalFullBuffer.size() - CurrentState.Buffer.size())
                    << " before true EOF.\nReplacing with SPACE (' ')..."
                    << std::endl);
      return ' ';

    case '\n':
    case '\r':
    {
      auto NextCh = static_cast<Char>(CurrentState.Buffer.front());
      // Consume whitespace. If observing a "\n\r" or "\r\n" sequence, ignore
      // the '\r' and report it as a single newline.
      if ((NextCh == '\n' || NextCh == '\r') && NextCh != Ch)
        CurrentState.Buffer.remove_prefix(1);
      return '\n';
    }
  }
}

lexer::Char lexer::peek_char() noexcept
{
  restore_guard G{CurrentState.Buffer};
  return get_char();
}

} // namespace monomux::tools::dto_compiler
