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

namespace monomux
{

/// Wrapper class over \p std::atomic that enables "moving" the value contained
/// by non-atomically initialising a \b new \p atomic with the current contained
/// value.
template <typename T> class MovableAtomic
{
  std::atomic<T> V = ATOMIC_VAR_INIT(T{});

public:
  MovableAtomic() = default;
  MovableAtomic(T&& Val) { V.store(std::forward<T>(Val)); }
  MovableAtomic(MovableAtomic&& RHS) noexcept { V.store(RHS.V.load()); }
  MovableAtomic& operator=(MovableAtomic&& RHS) noexcept
  {
    if (this == &RHS)
      return *this;

    V.store(RHS.V.load());
    return *this;
  }
  std::atomic<T>& get() noexcept { return V; }
  const std::atomic<T>& get() const noexcept { return V; }
};

} // namespace monomux
