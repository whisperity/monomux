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

namespace monomux
{
namespace client
{

/// A \p ControlClient is a non-user-facing management wrapper over an
/// established \p Client. It is used to implement special client-like
/// operations.
///
/// \note The control client \e wraps a \p Client instance, and as such, will
/// act as a separate client from the ones the user might have attached to the
/// session with.
class ControlClient
{
public:
  ControlClient(Client& C, std::string Session);

  const std::string& sessionName() const noexcept { return SessionName; }

  /// Send a request to the server to gracefully detach the latest (in terms of
  /// activity) client from the session.
  ///
  /// \see server::ClientData::lastActive()
  void requestDetachLatestClient();

  /// Send a request to the server to gracefully detach every client from the
  /// session.
  void requestDetachAllClients();

private:
  Client& BackingClient;

  /// The name of the session the controlling client will send requests to.
  std::string SessionName;
};

} // namespace client
} // namespace monomux
