/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "monomux/adt/FunctionExtras.hpp"

namespace monomux::tools::dto_compiler
{

/// Contains the kinds of tokens supported by the DTO DSL.
enum class token
{
  /// Default Token sentinel representing an unparsed token.
  NullToken,

#define TOKEN(NAME) NAME,
#include "Tokens.inc.h"

  /// DO NOT USE! Present only to be able to calculate how many TokenKinds are
  /// there.
  LastTokenSentinel
};

/// \returns a nice, human-readable name for the token kind.
std::string_view to_string(token TK);

template <token TK> struct token_info
{};

#define BEFORE_TOKEN(NAME)                                                     \
  template <> struct token_info<token::NAME>                                   \
  {                                                                            \
    std::string to_string() const;                                             \
    static constexpr token Kind = token::NAME;
#define TOKEN_INFO(TYPE, NAME) TYPE NAME;
#define AFTER_TOKEN(NAME)                                                      \
  }                                                                            \
  ;
#include "Tokens.inc.h"

#define TOKEN(NAME) token_info<token::NAME>
#define BEFORE_TOKEN(NAME) ,
using all_token_infos_type = std::variant<std::monostate
#include "Tokens.inc.h"
                                          >;

/// \returns a nice, human-readable formatted representation of the token
/// contained in \p Info.
std::string to_string(const all_token_infos_type& Info);

namespace detail
{

class char_sequence_lexer;

} // namespace detail

class lexer
{
  std::unique_ptr<detail::char_sequence_lexer> SequenceLexer;

  /// The original buffer the Lexer was constructed with.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  const std::string_view OriginalFullBuffer;

public:
  struct location
  {
    std::size_t Absolute;
    std::size_t Line, Column;

    /// Creates a \p location based on the position where \p Buffer begins
    /// within the \p FullBuffer.
    static location make_location(std::string_view FullBuffer,
                                  std::string_view Buffer) noexcept;

    /// Creates a \p location for the position marked by \p AbsoluteLoc in the
    /// \p Buffer.
    static location make_location(std::string_view Buffer,
                                  std::size_t AbsoluteLoc) noexcept;
  };

private:
  struct state
  {
    /// The currently lexed tail end of the buffer. Same as \p
    /// OriginalFullBuffer at the beginning.
    std::string_view Buffer;

    /// The last lexed token.
    token Tok;
    /// Information about the last lexed token, if any.
    ///
    /// This is \b guaranteed to contain the appropriate \p TokenKind
    /// specialisation.
    all_token_infos_type Info;

    /// The absolute location (first char in the buffer at index 0) where the
    /// last lexed token \p Tok begun.
    std::size_t Loc;
  };
  state CurrentState;

public:
  // NOLINTLEXCTLINE(readability-identifier-naming): 'char' would be a keyword.
  using Char = std::uint8_t;

  explicit lexer(std::string_view Buffer);
  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(lexer);
  ~lexer();

  /// Lexes the next token and returns its identifying kind. The state of the
  /// lexer is updated by this operation.
  [[nodiscard]] token lex() noexcept;

  /// Returns the type of the last lexed \p token without lexing a new one.
  [[nodiscard]] token current_token() const noexcept
  {
    return CurrentState.Tok;
  }

  /// \returns the \p location of the last \p token returned by \p lex().
  [[nodiscard]] location get_location() const noexcept;

  /// Lexes the next token and returns its identifying kind, but discards the
  /// result without affecting the state of the lexer.
  [[nodiscard]] token peek() noexcept;

  /// \returns the currently stored token info, without any semantic checks to
  /// its value.
  [[nodiscard]] const all_token_infos_type& get_token_info_raw() const noexcept
  {
    return CurrentState.Info;
  }

  /// \returns the \p token_info for the current, already lexed token if it is
  /// of kind \p TK.
  template <token TK>
  [[nodiscard]] std::optional<token_info<TK>> get_token_info() const noexcept
  {
    if (auto* TKInfo = std::get_if<token_info<TK>>(&CurrentState.Info))
      return *TKInfo;
    return std::nullopt;
  }

private:
  /// \returns the first "meaningful" (non-whitespace) character at the
  /// beginning of the to-be-read \p Buffer.
  [[nodiscard]] Char get_char() noexcept;

  /// \returns the first "meaningful" (non-whitespace) character at the
  /// beginning of the to-be-read \p Buffer without altering it.
  [[nodiscard]] Char peek_char() noexcept;

  /// Cut \p TokenBuffer to end at the already consumed portion of the \p Lexer
  /// read-buffer, optionally restoring \p KeepCharsAtEnd number of characters
  /// back to the end.
  void token_buffer_set_end_at_consumed_buffer(
    std::string_view& TokenBuffer, std::size_t KeepCharsAtEnd = 0) noexcept;

  /// Lexes the next token and return its identifying kind, mutating the state
  /// of the lexer in the process.
  [[nodiscard]] token lex_token() noexcept;

  /// Lexes a comment, either by ignoring it to a \p NullToken, or by creating
  /// a \p Comment token that contains the lexed value for the comment.
  [[nodiscard]] token lex_comment(std::string_view& TokenBuffer,
                                  bool MultiLine) noexcept;

  /// Lexes an integer literal from the pending \p TokenBuffer that starts with
  /// the \p Ch character.
  [[nodiscard]] token
  lex_integer_literal(std::string_view& TokenBuffer) noexcept;

  template <token TK, typename... Args>
  token set_current_token(Args&&... Argv) noexcept;
  token set_current_token_raw(token TK, all_token_infos_type Info) noexcept;
};

} // namespace monomux::tools::dto_compiler
