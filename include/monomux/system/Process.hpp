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
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

#include "monomux/system/CheckedPOSIX.hpp"
#include "monomux/system/Pty.hpp"

#include "monomux/Log.hpp"

namespace monomux
{

/// Responsible for creating, executing, and handling processes on the
/// system.
class Process
{
public:
  /// Type alias for the raw process handle type on the platform.
  using raw_handle = ::pid_t;

  static constexpr raw_handle Invalid = -1;

  struct SpawnOptions
  {
    std::string Program;
    std::vector<std::string> Arguments;
    std::map<std::string, std::optional<std::string>> Environment;

    bool CreatePTY = false;
  };

  raw_handle raw() const noexcept { return Handle; }
  bool hasPty() const noexcept { return PTY.has_value(); }
  Pty* getPty() noexcept { return hasPty() ? &*PTY : nullptr; }

  /// \returns Checks if the process had died, and if so, returns \p true.
  ///
  /// \note This call does not block. If the process died, the operating system
  /// \b MAY remove associated information at the invocation of this call.
  bool reapIfDead();

  /// Send the \p Signal to the underlying process.
  void signal(int Signal);

private:
  raw_handle Handle;
  /// The \p Pty assocaited with the process, if \p SpawnOptions::CreatePTY was
  /// true.
  std::optional<Pty> PTY;

public:
  /// Replaces the current process (as if by calling the \p exec() family) in
  /// the system with the started one. This is a low-level operation that
  /// performs no additional meaningful setup of process state.
  ///
  /// \warning This command does \b NOT \p fork()!
  [[noreturn]] static void exec(const SpawnOptions& Opts);

  /// Spawns a new process based on the specified \p Opts. This process calls
  /// \p fork() internally, and then does an \p exec(). The spawned subprocess
  /// will be meaningfully set up to be a clearly spawned process.
  ///
  /// The spawned process will be the child of the current process. The call
  /// returns the PID of the child, and execution resumes normally in the
  /// parent.
  ///
  /// \note This call does \b NOT return in the child!
  static Process spawn(const SpawnOptions& Opts);

  /// \p fork(): Ask the kernel to create an exact duplicate of the current
  /// process. The specified callbacks \p ParentAction and \p ChildAction will
  /// be run in the parent and the child process, respectively.
  ///
  /// \note Execution continues normally after the callbacks retur return!
  template <typename ParentFn, typename ChildFn>
  static void fork(ParentFn ParentAction, ChildFn ChildAction)
  {
    auto ForkResult = CheckedPOSIXThrow([] { return ::fork(); }, "fork()", -1);

    if (ForkResult == 0)
    {
      MONOMUX_TRACE_LOG(log::trace("system/Process") << "Forked, in child...");
      ChildAction();
    }
    else
    {
      MONOMUX_TRACE_LOG(log::trace("system/Process") << "Forked, in parent...");
      ParentAction();
    }
  }
};

using raw_pid = Process::raw_handle;

} // namespace monomux
