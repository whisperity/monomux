/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <sstream>

#include "monomux/adt/FunctionExtras.hpp"

#include "ast/decl.hpp"
#include "ast/type.hpp"

namespace monomux::tools::dto_compiler
{

class dto_unit
{
  ast::decl_context Root;

public:
  MONOMUX_MAKE_STRICT_TYPE(dto_unit, );

  [[nodiscard]] const ast::decl_context& get_root() const noexcept
  {
    return Root;
  }
  MONOMUX_MEMBER_0(ast::decl_context&, get_root, [[nodiscard]], noexcept);

  std::ostringstream dump() const;
};

} // namespace monomux::tools::dto_compiler
