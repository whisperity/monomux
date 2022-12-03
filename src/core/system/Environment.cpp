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
#include <utility>

#include "monomux/system/Environment.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Environment")

namespace monomux::system
{

std::string getEnv(const std::string& Key)
{
  const char* const Value = std::getenv(Key.c_str());
  if (!Value)
  {
    MONOMUX_TRACE_LOG(LOG(data) << "getEnv(" << Key << ") -> unset");
    return {};
  }
  MONOMUX_TRACE_LOG(LOG(data) << "getEnv(" << Key << ") = " << Value);
  return {Value};
}

std::vector<std::pair<std::string, std::string>>
MonomuxSession::createEnvVars() const
{
  std::vector<std::pair<std::string, std::string>> R;
  R.emplace_back(std::make_pair("MONOMUX_SOCKET", Socket.to_string()));
  R.emplace_back(std::make_pair("MONOMUX_SESSION", SessionName));
  return R;
}

std::optional<MonomuxSession> MonomuxSession::loadFromEnv()
{
  std::string SocketPath = getEnv("MONOMUX_SOCKET");
  std::string SessionName = getEnv("MONOMUX_SESSION");

  if (SocketPath.empty() || SessionName.empty())
    return std::nullopt;

  LOG(data) << "Session from environment:\n\tServer socket: " << SocketPath
            << "\n\tSession name: " << SessionName;

  MonomuxSession S;
  S.Socket.Filename = SocketPath;
  S.SessionName = std::move(SessionName);
  return S;
}

} // namespace monomux::system

#undef LOG
