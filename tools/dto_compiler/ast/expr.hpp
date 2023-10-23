/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <sstream>

#include "monomux/adt/FunctionExtras.hpp"

namespace monomux::tools::dto_compiler::ast
{

class expr
{
public:
  MONOMUX_MAKE_STRICT_TYPE(expr, virtual);

  void dump(std::size_t Depth = 0) const;
  virtual void dump(std::ostringstream& OS, std::size_t Depth) const;
};

#define MONOMUX_EXPR_DUMP                                                      \
  void dump(std::ostringstream& OS, std::size_t Depth) const override;

class unsigned_integral_literal : public expr
{
  unsigned long long Value;

public:
  MONOMUX_EXPR_DUMP;
  MONOMUX_MAKE_NON_COPYABLE_MOVABLE(unsigned_integral_literal);
  explicit unsigned_integral_literal(unsigned long long Value) : Value(Value) {}
  ~unsigned_integral_literal() override = default;

  [[nodiscard]] unsigned long long get_value() const noexcept { return Value; }
};

#undef MONOMUX_EXPR_DUMP

} // namespace monomux::tools::dto_compiler::ast
