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
#include "Client.hpp"

#include <optional>
#include <string>
#include <vector>

namespace monomux
{
namespace client
{

/// Options interested to invocation of a Monomux Client.
struct Options
{
  /// Format the options back into the CLI invocation they were parsed from.
  std::vector<std::string> toArgv() const;

  // (To initialise the bitfields...)
  Options() : ClientMode(false), ForceSessionSelectMenu(false) {}

  /// Whether the client mode was enabled.
  bool ClientMode : 1;

  /// Whether the client should start with showing the session selection menu,
  /// and disregard normal startup decision heuristics.
  bool ForceSessionSelectMenu : 1;

  /// The path to the server socket where the client should connect to.
  std::optional<std::string> SocketPath;

  /// The name of the session the client should create if does not exist, or
  /// attach to if exists.
  std::optional<std::string> SessionName;

  /// The program to start if a new session is created during the client's
  /// connection.
  std::optional<std::string> Program;

  /// The command-line arguments to give to the invoked program during session
  /// start.
  std::vector<std::string> ProgramArgs;

  /// Contains the master connection to the server, if such was established.
  std::optional<Client> Connection;
};

/// Attempt to establish connection to a Monomux Server specified in \p Opts.
///
/// \param Block Whether to continue retrying the connection and block until
/// success.
/// \param FailureReason If given, after an unsuccessful connection, a
/// human-readable reason for the failure will be written to.
std::optional<Client>
connect(Options& Opts, bool Block, std::string* FailureReason);

/// Executes the Monomux Client logic.
///
/// \returns \p ExitCode
int main(Options& Opts);

} // namespace client
} // namespace monomux
