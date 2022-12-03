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
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "monomux/system/CurrentPlatform.hpp"
#include "monomux/system/ProcessTraits.hpp"
#include "monomux/system/Pty.hpp"

namespace monomux::system
{

using PlatformSpecificProcessTraits = ProcessTraits<CurrentPlatform>;

/// Responsible for creating, executing, and handling processes on the
/// system.
class Process
{
public:
  /// Type alias for the raw process handle type on the platform.
  using Raw = PlatformSpecificProcessTraits::RawTy;

  struct SpawnOptions
  {
    std::string Program;
    std::vector<std::string> Arguments;
    std::map<std::string, std::optional<std::string>> Environment;

    /// Whether to create a pseudoterminal device when creating the process.
    bool CreatePTY = false;
    /// Override the standard streams of the spawned process to the
    /// file descriptors specified. If \p fd::Invalid is given, the potentially
    /// inherited standard stream will be closed.
    ///
    /// This option has no effect if \p CreatePTY is \p true.
    std::optional<Handle::Raw> StandardInput, StandardOutput, StandardError;
  };

  virtual ~Process() = default;

  Raw raw() const noexcept { return Handle; }
  bool hasPty() const noexcept { return static_cast<bool>(PTY); }
  Pty* getPty() noexcept { return hasPty() ? &*PTY : nullptr; }

  /// \returns Checks if the process had died, and if so, returns \p true.
  ///
  /// \note This call does not block. If the process died, the operating system
  /// \b MAY remove associated information at the invocation of this call.
  virtual bool reapIfDead() = 0;

  /// Blocks until the current process instance has terminated.
  virtual void wait() = 0;

  /// \returns whether the child process has been \b OBSERVED to be dead.
  bool dead() const noexcept { return Dead; }

  /// \returns the exit code of the process, if it has already terminated.
  /// This call is only valid after \p wait() concluded or \p reapIfDead()
  /// returns \p true.
  int exitCode() const noexcept
  {
    assert(Dead && "Process still alive, exit code is not meaningful!");
    return ExitCode;
  }

  /// Send the \p Signal to the underlying process.
  virtual void signal(int Signal) = 0;

protected:
  Raw Handle = PlatformSpecificProcessTraits::Invalid;
  bool Dead = false;
  int ExitCode = 0;
  /// The \p Pty assocaited with the process, if \p SpawnOptions::CreatePTY was
  /// true.
  std::unique_ptr<Pty> PTY;

public:
  /// \returns the PID handle of the currently executing process.
  static Raw thisProcess();

  /// \returns the address of the currently executing binary, queried from the
  /// kernel.
  static std::string thisProcessPath();

  /// Sends the \p Signal to the process identified by \p PID.
  static void signal(Raw Handle, int Signal);

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
  static std::unique_ptr<Process> spawn(const SpawnOptions& Opts);
};

} // namespace monomux::system
