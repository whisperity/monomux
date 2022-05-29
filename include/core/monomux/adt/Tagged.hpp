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
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace monomux
{

/// Tags a pointer to a type at compile-time with a scalar value that is only
/// descript to clients consuming this object.
/// Otherwise, behaves the same as \p T*.
template <std::size_t N, typename T> class Tagged
{
  static constexpr std::size_t Kind = N;
  T* Ptr;

public:
  Tagged(T* P) noexcept : Ptr(P) {}

  /// Retrieve the raw tag value.
  std::size_t kind() const noexcept { return Kind; }
  /// Retrieve the tag value cast to the enum type \p E.
  template <typename E> E kindAs() const noexcept
  {
    return static_cast<E>(Kind);
  }

  bool operator==(Tagged RHS) const { return Ptr == RHS.Ptr; }
  bool operator!=(Tagged RHS) const { return Ptr != RHS.Ptr; }

  T* operator->() noexcept { return Ptr; }
  const T* operator->() const noexcept { return Ptr; }
  T& operator*() noexcept
  {
    assert(Ptr);
    return *Ptr;
  }
  const T& operator*() const noexcept
  {
    assert(Ptr);
    return *Ptr;
  }
  T* get() noexcept { return Ptr; }
  const T* get() const noexcept { return Ptr; }
};

} // namespace monomux
