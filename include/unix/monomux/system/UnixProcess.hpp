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
#include <unistd.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/system/Process.hpp"

namespace monomux::system::unix
{

/// Responsible for creating, executing, and handling processes on the
/// system.
class Process : public system::Process
{
public:
  bool reapIfDead() override;

  void wait() override;

  void signal(int Signal) override;

  /// \p fork(): Ask the kernel to create an exact duplicate of the current
  /// process. The specified callbacks \p ParentAction and \p ChildAction will
  /// be run in the parent and the child process, respectively.
  ///
  /// \note Execution continues normally after the callbacks retur return!
  template <typename ParentFn, typename ChildFn>
  static void fork(ParentFn ParentAction, ChildFn ChildAction)
  {
    auto ForkResult = CheckedErrnoThrow([] { return ::fork(); }, "fork()", -1);
    if (ForkResult == 0)
      ChildAction();
    else
      ParentAction();
  }
};

} // namespace monomux::system::unix
