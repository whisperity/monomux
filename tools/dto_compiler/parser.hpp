/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"

#include "lexer.hpp"

namespace monomux::tools::dto_compiler
{

class dto_unit;

namespace ast
{

class decl_context;
class namespace_decl;

} // namespace ast

/// Consumes the \p token stream emitted by the \p lexer to build a
/// \p dto_unit.
class parser
{
public:
  struct error_info
  {
    lexer::location Location;
    token TokenKind;
    all_token_infos_type TokenInfo;

    std::string Reason;
  };

private:
  lexer& Lexer; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members).
  std::unique_ptr<dto_unit> ParseUnit;

  std::optional<error_info> Error;
  void set_error_to_current_token(std::string Reason) noexcept;

  [[nodiscard]] token get_current_token() noexcept;
  [[nodiscard]] token get_next_token() noexcept;

  ast::decl_context* DeclContext;

  std::vector<std::string> parse_potentially_scoped_identifier();
  bool parse_namespace();

public:
  explicit parser(lexer& Lexer);
  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(parser);
  ~parser();

  [[nodiscard]] std::unique_ptr<dto_unit> take_unit() && noexcept
  {
    return std::move(ParseUnit);
  }

  [[nodiscard]] const dto_unit& get_unit() const noexcept { return *ParseUnit; }
  MONOMUX_MEMBER_0(dto_unit&, get_unit, [[nodiscard]], noexcept);

  /// Parses a definition set from the \p Lexer according to the language rules
  /// baked into the instance.
  ///
  /// \returns whether the parsing was successful.
  bool parse();

  [[nodiscard]] bool has_error() const noexcept { return Error.has_value(); }
  [[nodiscard]] const error_info& get_error() const noexcept;
};

} // namespace monomux::tools::dto_compiler
