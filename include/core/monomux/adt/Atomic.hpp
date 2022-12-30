/**
 * Copyright (C) 2022 Whisperity
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <atomic>
#include <type_traits>

namespace monomux
{

/// Wrapper class over \p std::atomic<T> that enables "copying" and "moving" the
/// value contained by non-atomically initialising a \b new \p atomic with the
/// current contained value.
template <typename T> class Atomic
{
  using AT = std::atomic<T>;
  static constexpr bool IsNoExcept = noexcept(
    std::declval<AT>().store(T{}))&& noexcept(std::declval<AT>().load());

  AT Value = ATOMIC_VAR_INIT(T{});

public:
  Atomic() noexcept(std::is_nothrow_default_constructible_v<AT>) = default;
  Atomic(T&& Val) noexcept(IsNoExcept) { Value.store(std::forward<T>(Val)); }
  Atomic(const Atomic& RHS) noexcept(IsNoExcept)
  {
    Value.store(RHS.Value.load());
  }
  Atomic(Atomic&& RHS) noexcept(IsNoExcept) { Value.store(RHS.Value.load()); }
  Atomic& operator=(const Atomic& RHS) noexcept(IsNoExcept)
  {
    if (this == &RHS)
      return *this;

    Value.store(RHS.Value.load());
    return *this;
  }
  Atomic& operator=(Atomic&& RHS) noexcept(IsNoExcept)
  {
    if (this == &RHS)
      return *this;

    Value.store(RHS.Value.load());
    return *this;
  }
  ~Atomic() = default;

  [[nodiscard]] std::atomic<T>& get() noexcept { return Value; }
  [[nodiscard]] const std::atomic<T>& get() const noexcept { return Value; }
};

} // namespace monomux
