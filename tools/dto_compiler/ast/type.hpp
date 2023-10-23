/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <string>

#include "monomux/adt/FunctionExtras.hpp"

namespace monomux::tools::dto_compiler
{

class dto_unit;

} // namespace monomux::tools::dto_compiler

namespace monomux::tools::dto_compiler::ast
{

class type
{
  std::string Identifier;

protected:
  explicit type(std::string Identifier) : Identifier(std::move(Identifier)) {}

public:
  static type* try_as_conjured(dto_unit& Unit, std::string Identifier);

  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(type);
  virtual ~type() = default;

  void dump(std::size_t Depth = 0) const;
  virtual void dump(std::ostringstream& OS, std::size_t Depth) const;

  [[nodiscard]] const std::string& get_identifier() const noexcept
  {
    return Identifier;
  }
};

#define MONOMUX_TYPE_DUMP                                                      \
  void dump(std::ostringstream& OS, std::size_t Depth) const override;

class integral_type : public type
{
  std::string GeneratedIdentifier;

protected:
  integral_type(std::string Identifier, std::string GeneratedIdentifier)
    : type(std::move(Identifier)),
      GeneratedIdentifier(std::move(GeneratedIdentifier))
  {}

public:
  static integral_type* conjure(dto_unit& Unit, std::string Identifier);

  MONOMUX_TYPE_DUMP;
  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(integral_type);
  ~integral_type() override = default;

  [[nodiscard]] const std::string& get_generated_identifier() const noexcept
  {
    return GeneratedIdentifier;
  }
};

#undef MONOMUX_TYPE_DUMP

} // namespace monomux::tools::dto_compiler::ast
