/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <chrono>
#include <string>

namespace monomux::client
{

/// A snaphot view of sessions running on a server, as reported by the server.
///
/// \see monomux::message::SessionData
/// \see monomux::server::SessionData
struct SessionData
{
  std::string Name;
  std::chrono::time_point<std::chrono::system_clock> Created;
};

} // namespace monomux::client
