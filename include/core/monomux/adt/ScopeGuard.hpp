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
#include "monomux/adt/UniqueScalar.hpp"

namespace monomux
{

/// A simple scope guard that fires a callback function (in most cases, a
/// lambda passed to the constructor) when constructed, and when destructed.
///
/// Example:
///
///   \code{.cpp}
///   scope_guard RAII{[] { enter(); }, [] { exit(); }};
///   \endcode
template <typename EnterFunction, typename ExitFunction> struct ScopeGuard
{
  ScopeGuard(EnterFunction&& Enter, ExitFunction&& Exit) : Exit(Exit)
  {
    Enter();
    Alive = true;
  }

  ~ScopeGuard()
  {
    if (Alive)
      Exit();
  }

  ScopeGuard() = delete;
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard(ScopeGuard&&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;

private:
  bool Alive;
  ExitFunction Exit;
};

} // namespace monomux
