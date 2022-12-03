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

#include "monomux/system/Platform.hpp"

namespace monomux::system
{

/// \returns the value of the environment variable \p Key.
///
/// \note This function is a safe alternative to \p getenv() as it immediately
/// allocates a \e new string with the result.
std::string getEnv(const std::string& Key);

/// Allows crafting and retrieving information about a running Monomux session
/// injected through the use of environment variables.
struct MonomuxSession
{
  Platform::SocketPath Socket;
  std::string SessionName;

  std::vector<std::pair<std::string, std::string>> createEnvVars() const;
  static std::optional<MonomuxSession> loadFromEnv();
};

} // namespace monomux::system
