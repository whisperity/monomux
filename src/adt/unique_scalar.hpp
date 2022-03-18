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
#include <utility>

namespace monomux
{

/// Wraps a movable scalar type which resets to the default value when
/// moved-from. Unlike \p unique_ptr, the value is allocated in-place.
template <typename T, T Default>
// NOLINTNEXTLINE(readability-identifier-naming): Mimicking unique_ptr.
struct unique_scalar
{
  /// Initialises the object with the default value.
  unique_scalar() noexcept : Value(Default) {}
  /// Initialises the object with the specified value
  unique_scalar(T Value) : Value(Value) {}
  /// Move-initialises the value from \p RHS, and resets \p RHS to the
  /// \p Default.
  unique_scalar(unique_scalar&& RHS) noexcept : Value(std::move(RHS.Value))
  {
    RHS.Value = Default;
  }
  /// Move-assigns the value from \p RHS, and resets \p RHS to the \p Default.
  unique_scalar& operator=(unique_scalar&& RHS) noexcept
  {
    if (this == &RHS)
      return *this;

    Value = std::move(RHS.Value);
    RHS.Value = Default;
    return *this;
  }
  ~unique_scalar() = default;

  /// Converts to the stored value.
  operator T&() { return Value; }
  /// Converts to the stored value.
  operator const T&() const { return Value; }

  /// Sets \p NewValue into the wrapped object.
  unique_scalar& operator=(T&& NewValue) noexcept
  {
    Value = std::forward<T>(NewValue);
    return *this;
  }

private:
  T Value;
};

} // namespace monomux
