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

#include "system/Socket.hpp"

#include <memory>
#include <optional>

namespace monomux
{
namespace server
{

/// Stores information about and associated resources to a connected client.
class ClientData
{
public:
  ClientData(std::unique_ptr<Socket> Connection);

  std::size_t id() const noexcept { return ID; }
  /// Returns the most recent random-generated nonce for this client, and
  /// clear it from memory, rendering it useless in later authentications.
  std::size_t consumeNonce() noexcept;
  /// Creates a new random number for the client's authentication, and returns
  /// it. The value is stored for until used.
  std::size_t makeNewNonce() noexcept;

  Socket& getControlSocket() noexcept { return *ControlConnection; }
  Socket* getDataSocket() noexcept { return DataConnection.get(); }

  /// Releases the control socket of the other client and associates it as the
  /// data connection of the current client.
  void subjugateIntoDataSocket(ClientData& Other) noexcept;

private:
  std::size_t ID;
  std::optional<std::size_t> Nonce;

  /// The control connection transcieves control information and commands.
  std::unique_ptr<Socket> ControlConnection;

  /// The data connection transcieves the actual program data.
  std::unique_ptr<Socket> DataConnection;
};

} // namespace server
} // namespace monomux
