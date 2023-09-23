/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace dto_compiler
{

/// Contains the kinds of tokens supported by the DTO DSL.
enum class Token
{
  NullToken,

#define TOKEN(NAME) NAME,
#include "Tokens.inc.h"

  LastTokenSentinel
};

/// \returns a nice, human-readable name for the token kind.
std::string_view tokNiceName(Token TK);

template <Token TK> struct TokenInfo
{};

#define BEFORE_TOKEN(NAME)                                                     \
  template <> struct TokenInfo<Token::NAME>                                    \
  {                                                                            \
    std::string toString() const;                                              \
    static constexpr Token Kind = Token::NAME;
#define TOKEN_INFO(TYPE, NAME) TYPE NAME;
#define AFTER_TOKEN(NAME)                                                      \
  }                                                                            \
  ;
#include "Tokens.inc.h"

#define TOKEN(NAME) TokenInfo<Token::NAME>
#define BEFORE_TOKEN(NAME) ,
using AllTokenInfos = std::variant<std::monostate
#include "Tokens.inc.h"
                                   >;

/// \returns a nice, human-readable formatted representation of the token
/// contained in \p Info.
std::string tokToString(const AllTokenInfos& Info);

namespace detail
{

class CharSequenceLexer;

} // namespace detail

class Lexer
{
  std::unique_ptr<detail::CharSequenceLexer> SeqLexer;

  /// The original buffer the Lexer was constructed with.
  const std::string_view OriginalFullBuffer;

  struct State
  {
    /// The currently lexed tail end of the buffer. Same as \p
    /// OriginalFullBuffer at the beginning.
    std::string_view Buffer;

    /// The last lexed token.
    Token Tok;
    /// Information about the last lexed token, if any.
    /// This is \b guaranteed to contain the appropriate \p TokenKind
    /// specialisation.
    AllTokenInfos Info;
  };
  State CurrentState;

public:
  using Char = std::uint8_t;

  explicit Lexer(std::string_view Buffer);
  Lexer(const Lexer&) = delete;
  Lexer(Lexer&&) = delete;
  Lexer& operator=(const Lexer&) = delete;
  Lexer& operator=(Lexer&&) = delete;
  ~Lexer();

  /// Lexes the next token and returns its identifying kind. The state of the
  /// lexer is updated by this operation.
  [[nodiscard]] Token lex();

  /// Lexes the next token and returns its identifying kind, but discards the
  /// result without affecting the state of the lexer.
  [[nodiscard]] Token peek();

  [[nodiscard]] const AllTokenInfos& getTokenInfoRaw() const noexcept
  {
    return CurrentState.Info;
  }

  /// Retrieves the \p TokenInfo for the current, already lexed token if it is
  /// of kind \p TK.
  template <Token TK>
  [[nodiscard]] std::optional<TokenInfo<TK>> getTokenInfo() const noexcept
  {
    if (auto* TKInfo = std::get_if<TokenInfo<TK>>(CurrentState.Info))
      return *TKInfo;
    return std::nullopt;
  }

private:
  /// \returns the first "meaningful" (non-whitespace) character at the
  /// beginning of the to-be-read \p Buffer.
  [[nodiscard]] Char getChar() noexcept;

  /// \returns the first "meaningful" (non-whitespace) character at the
  /// beginning of the to-be-read \p Buffer without altering it.
  [[nodiscard]] Char peekChar() noexcept;

  /// Cut \p TokenBuffer to end at the already consumed portion of the \p Lexer
  /// read-buffer, optionally restoring \p KeepCharsAtEnd number of characters
  /// back to the end.
  void tokenBufferSetEndAtReadBuffer(std::string_view& TokenBuffer,
                                     std::size_t KeepCharsAtEnd = 0);

  /// Lexes the next token and return its identifying kind, mutating the state
  /// of the lexer in the process.
  [[nodiscard]] Token lexToken();

  /// Lexes an integer literal from the pending \p TokenBuffer that starts with
  /// the \p Ch character.
  [[nodiscard]] Token lexIntegerlLiteral(std::string_view& TokenBuffer);

  template <Token TK, typename... Args> Token setCurrentToken(Args&&... Argv);
  Token setCurrentTokenRaw(Token TK, AllTokenInfos Info);
};

} // namespace dto_compiler
