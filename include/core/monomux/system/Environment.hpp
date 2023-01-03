/* SPDX-License-Identifier: LGPL-3.0-only */
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
[[nodiscard]] std::string getEnv(const std::string& Key);

/// Allows crafting and retrieving information about a running Monomux session
/// injected through the use of environment variables.
struct MonomuxSession
{
  Platform::SocketPath Socket;
  std::string SessionName;

  [[nodiscard]] std::vector<std::pair<std::string, std::string>>
  createEnvVars() const;
  [[nodiscard]] static std::optional<MonomuxSession> loadFromEnv();
};

} // namespace monomux::system
