/* SPDX-License-Identifier: LGPL-3.0-only */
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
