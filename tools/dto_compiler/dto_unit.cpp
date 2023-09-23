/* SPDX-License-Identifier: LGPL-3.0-only */
#include <sstream>

#include "dto_unit.hpp"

namespace monomux::tools::dto_compiler
{

std::ostringstream dto_unit::dump() const
{
  std::ostringstream Ret;

  Ret << "DTOContext " << this << '\n';
  Root.dump_children(Ret, 1);

  return Ret;
}

} // namespace monomux::tools::dto_compiler
