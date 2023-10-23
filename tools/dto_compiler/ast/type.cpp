/* SPDX-License-Identifier: LGPL-3.0-only */
#include <string>

#include "../dto_unit.hpp"

#include "type.hpp"

namespace monomux::tools::dto_compiler::ast
{

type* type::try_as_conjured(dto_unit& Unit, std::string Identifier)
{
  return integral_type::conjure(Unit, std::move(Identifier));
}

integral_type* integral_type::conjure(dto_unit& Unit, std::string Identifier)
{
#define MAKE(ID, PREAMBLE, GEN_ID)                                             \
  if (Identifier == ID)                                                        \
  {                                                                            \
    Unit.add_to_preamble(PREAMBLE, PREAMBLE, PREAMBLE);                        \
    return new integral_type{ID, GEN_ID};                                      \
  }

  MAKE("ui64", "#include <cstdint>", "std::uint64_t");

#undef MAKE

  return nullptr;
}

} // namespace monomux::tools::dto_compiler::ast
