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
#include <functional>
#include <tuple>

#include "unique_scalar.hpp"

namespace monomux
{

/// An abstract scope guard that fires two callbacks of an object at the
/// beginning and the end of its own life.
template <typename Function, typename Object> class MemberFnScopeGuard0
{
  unique_scalar<bool, false> Alive;
  Function Enter;
  Function Exit;
  Object* O;

public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  MemberFnScopeGuard0(Function Enter, Function Exit, Object* Obj)
    : Enter(Enter), Exit(Exit), O(Obj)
  {
    if (Enter)
      (this->O->*Enter)();
    Alive = true;
  }

  ~MemberFnScopeGuard0()
  {
    if (Alive && Exit)
      (this->O->*Exit)();
    Alive = false;
  }
};

/// An abstract scope guard that fires two callbacks of an object at the
/// beginning and the end of its own life.
template <typename Function, typename Object, typename Arg1Type>
class MemberFnScopeGuard1
{
  unique_scalar<bool, false> Alive;
  Function Enter;
  Function Exit;
  Object* O;
  Arg1Type Arg1;

public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  MemberFnScopeGuard1(Function Enter, Function Exit, Object* Obj, Arg1Type Arg1)
    : Enter(Enter), Exit(Exit), O(Obj), Arg1(Arg1)
  {
    if (Enter)
      (this->O->*Enter)(Arg1);
    Alive = true;
  }

  ~MemberFnScopeGuard1()
  {
    if (Alive && Exit)
      (this->O->*Exit)(Arg1);
    Alive = false;
  }
};

} // namespace monomux

#define MONOMUX_MEMBER_FN_SCOPE_GUARD_0(                                       \
  CLASS_NAME, FUNCTION_TYPE, OBJECT_TYPE)                                      \
  class CLASS_NAME                                                             \
    : public monomux::MemberFnScopeGuard0<FUNCTION_TYPE, OBJECT_TYPE>          \
  {                                                                            \
  public:                                                                      \
    CLASS_NAME(FUNCTION_TYPE Enter, FUNCTION_TYPE Exit, OBJECT_TYPE* Obj)      \
      : MemberFnScopeGuard0(Enter, Exit, Obj)                                  \
    {}                                                                         \
                                                                               \
    ~CLASS_NAME() = default;                                                   \
  };

#define MONOMUX_MEMBER_FN_SCOPE_GUARD_1(                                       \
  CLASS_NAME, FUNCTION_TYPE, OBJECT_TYPE, ARG_1_TYPE)                          \
  class CLASS_NAME                                                             \
    : public monomux::                                                         \
        MemberFnScopeGuard1<FUNCTION_TYPE, OBJECT_TYPE, ARG_1_TYPE>            \
  {                                                                            \
  public:                                                                      \
    CLASS_NAME(FUNCTION_TYPE Enter,                                            \
               FUNCTION_TYPE Exit,                                             \
               OBJECT_TYPE* Obj,                                               \
               ARG_1_TYPE Arg1)                                                \
      : MemberFnScopeGuard1(Enter, Exit, Obj, Arg1)                            \
    {}                                                                         \
                                                                               \
    ~CLASS_NAME() = default;                                                   \
  };
