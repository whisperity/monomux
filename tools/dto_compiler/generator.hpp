/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <ostream>

#include "monomux/adt/FunctionExtras.hpp"

namespace monomux::tools::dto_compiler
{

class dto_unit;

/// Consumes the information available in a \p dto_unit and emits C++ source
/// code for the serialisation and communication protocol described in the DSL.
class generator
{
  std::unique_ptr<dto_unit> DTO;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::ostream& InterfaceOut;
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::ostream& ImplementationOut;

public:
  generator(std::unique_ptr<dto_unit>&& DTO,
            std::ostream& InterfaceStream,
            std::ostream& ImplementationStream);
  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(generator);
  ~generator() = default;

  void generate();
};

} // namespace monomux::tools::dto_compiler
