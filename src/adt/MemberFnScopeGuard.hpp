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

#include "unique_scalar.hpp"

#include <functional>
#include <tuple>

namespace monomux
{

/// An abstract scope guard that fires two callbacks of an object at the
/// beginning and the end of its own life.
template <typename Function, typename Object> class MemberFnScopeGuard
{
  unique_scalar<bool, false> Alive;
  Function Enter;
  Function Exit;
  Object* O;

public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  MemberFnScopeGuard(Function Enter, Function Exit, Object* Obj)
    : Enter(Enter), Exit(Exit), O(Obj)
  {
    (this->O->*Enter)();
    Alive = true;
  }

  ~MemberFnScopeGuard()
  {
    if (Alive)
      (this->O->*Exit)();
    Alive = false;
  }
};

} // namespace monomux

#define MONOMUX_MEMBER_FN_SCOPE_GAURD(CLASS_NAME, FUNCTION_TYPE, OBJECT_TYPE)  \
  class CLASS_NAME                                                             \
    : public monomux::MemberFnScopeGuard<FUNCTION_TYPE, OBJECT_TYPE>           \
  {                                                                            \
  public:                                                                      \
    CLASS_NAME(FUNCTION_TYPE Enter, FUNCTION_TYPE Exit, OBJECT_TYPE* Obj)      \
      : MemberFnScopeGuard(Enter, Exit, Obj)                                   \
    {}                                                                         \
                                                                               \
    ~CLASS_NAME() = default;                                                   \
  };
