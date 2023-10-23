/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <sstream>
#include <unordered_set>

#include "monomux/adt/FunctionExtras.hpp"

#include "ast/decl.hpp"
#include "ast/type.hpp"

namespace monomux::tools::dto_compiler
{

class dto_unit
{
  ast::decl_context Root;

  std::unordered_set<std::string> UsedPreambleTokens;
  std::ostringstream InterfacePreamble;
  std::ostringstream ImplementationPreamble;

public:
  MONOMUX_MAKE_STRICT_TYPE(dto_unit, );

  [[nodiscard]] const ast::decl_context& get_root() const noexcept
  {
    return Root;
  }
  MONOMUX_MEMBER_0(ast::decl_context&, get_root, [[nodiscard]], noexcept);

  void dump() const;
  void dump(std::ostringstream& OS) const;

  [[nodiscard]] std::string get_interface_preamble() const
  {
    return InterfacePreamble.str();
  }
  [[nodiscard]] std::string get_implementation_preamble() const
  {
    return ImplementationPreamble.str();
  }

  /// Adds the contents of \p Interface and \p Implementation to the respective
  /// preambles if and only if the supposedly unique \p Token has not been used
  /// for an \p add_to_preamble() call.
  void add_to_preamble(std::string Token,
                       const std::string& Interface,
                       const std::string& Implementation);
};

} // namespace monomux::tools::dto_compiler
