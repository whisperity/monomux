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

namespace monomux
{

/// Responsible for creating, executing, and handling processes on the
/// system.
class Process
{
public:
  using handle = ::pid_t;

  struct SpawnOptions
  {
    std::string Program;
    std::vector<std::string> Arguments;
    std::map<std::string, std::optional<std::string>> Environment;

    bool CreatePTY = false;
  };

  handle Handle;

  /// Replaces the current process (as if by calling the \p exec() family) in
  /// the system with the started one.
  ///
  /// \note WARNING: This command does *NOT* \p fork()!
  [[noreturn]] static void exec(const SpawnOptions& Opts);

  /// Spawns a new process based on the specified \p Opts. This process calls
  /// \p fork() internally, and then does an \p exec().
  ///
  /// The spawned process will be the child of the current process. The call
  /// returns the PID of the child, and execution resumes normally in the
  /// parent.
  ///
  /// \note This call does NOT return in the child!
  static Process spawn(const SpawnOptions& Opts);
};

} // namespace monomux
