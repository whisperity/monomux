/* SPDX-License-Identifier: LGPL-3.0-only */
#include <iostream>
#include <sstream>

#include "dto_unit.hpp"

namespace monomux::tools::dto_compiler
{

void dto_unit::dump() const
{
  std::ostringstream OS;

  OS << "DTOContext " << this << '\n';
  Root.dump_children(OS, 1);

  std::cerr << OS.str() << std::endl;
}

void dto_unit::dump(std::ostringstream& OS) const
{
  OS << "DTOContext " << this << '\n';
  Root.dump_children(OS, 1);
}

void dto_unit::add_to_preamble(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  std::string Token,
  const std::string& Interface,
  const std::string& Implementation)
{
  auto Insert = UsedPreambleTokens.insert(std::move(Token));
  if (!Insert.second)
    return;

  InterfacePreamble << Interface << '\n';
  ImplementationPreamble << Implementation << '\n';
}

} // namespace monomux::tools::dto_compiler
