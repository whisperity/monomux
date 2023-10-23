/* SPDX-License-Identifier: LGPL-3.0-only */
#include <cstdint>
#include <sstream>

#include "decl.hpp"

namespace monomux::tools::dto_compiler::ast
{

inline void print_ident(std::ostringstream& OS, std::size_t Indent)
{
  if (Indent == 0)
    OS << '.';

  while (Indent > 1)
  {
    OS << "|  ";
    --Indent;
  }
  if (Indent == 1)
  {
    OS << "|- ";
  }
}

} // namespace monomux::tools::dto_compiler::ast
