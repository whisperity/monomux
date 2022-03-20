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
#include <optional>
#include <string>
#include <vector>

namespace monomux
{
namespace server
{

/// Options interested to invocation of a Monomux Server.
struct Options
{
  /// Format the options back into the CLI invocation they were parsed from.
  std::vector<std::string> toArgv() const;

  // (To initialise the bitfields...)
  Options();

  /// Whether the server mode was enabled.
  bool ServerMode : 1;

  /// Whether the server should run as a background process.
  bool Background : 1;

  /// Whether the server should automatically quit if the last session running
  /// has terminated.
  bool ExitOnLastSessionTerminate : 1;

  /// The path of the server socket to start listening on.
  std::optional<std::string> SocketPath;
};

/// \p exec() into a server process that is created with the \p Opts options.
[[noreturn]] void exec(const Options& Opts, const char* ArgV0);

/// Executes the Monomux Server logic.
///
/// \returns \p ExitCode
int main(Options& Opts);

} // namespace server
} // namespace monomux
