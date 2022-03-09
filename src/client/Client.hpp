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

#include "system/Process.hpp"
#include "system/Socket.hpp"

#include <cassert>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace monomux
{

/// This class represents a connection to a running \p Server - or rather,
/// a wrapper over the communication channel that allows talking to the
/// server.
///
/// In networking terminology, this is effectively a client, but we reserve
/// that word for the "Monomux Client" which deals with attaching to a
/// process.
class Client
{
public:
  /// Creates a new connection client to the server at the specified socket.
  static std::optional<Client> create(std::string SocketPath);

  /// Initialise a \p Client over the already established \p ControlSocket.
  Client(Socket&& ControlSocket);

  /// Perform a handshake mechanism over the control socket.
  ///
  /// \return Whether the handshake process succeeded.
  bool handshake();

  // TODO: Document this.
  void requestSpawnProcess(const Process::SpawnOptions& Opts);

private:
  /// The control socket is used to communicate control commands with the
  /// server.
  Socket ControlSocket;

  /// The data connection is used to transmit the process data to the client.
  /// (This is initialised in a lazy fashion during operation.)
  std::unique_ptr<Socket> DataSocket;

  /// A unique identifier of the current \p Client, as returned by the server.
  std::size_t ClientID = -1;

  /// A unique, random generated single-use number, which the \p Client can
  /// use to establish its identity towards the server in another request.
  std::optional<std::size_t> Nonce;

  /// Return the stored \p Nonce of the current instance, resetting it.
  std::size_t consumeNonce() noexcept;

private:
  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<void(std::string_view)>> Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(std::string_view Message);
#include "Client.Dispatch.ipp"
};

} // namespace monomux
