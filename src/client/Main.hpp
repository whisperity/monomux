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

  /// Whether the client mode was enabled.
  bool ClientMode : 1;

  /// Contains the master connection to the server, if such was established.
  std::optional<Client> Connection;
};

/// Attempt to establish connection to a Monomux Server specified in \p Opts.
///
/// \param Block Whether to continue retrying the connection and block until
/// success.
std::optional<Client> connect(const Options& Opts, bool Block);

/// Executes the Monomux Client logic.
int main(Options& Opts);

} // namespace client
} // namespace monomux
