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
#include "system/epoll.hpp"
#include "system/fd.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace monomux
{

/// The monomux server is responsible for creating child processes of sessions.
/// Clients communicate with a \p Server instance to obtain information about
/// a session and to initiate attachment procedures.
class Server
{
public:
  /// Stores information about and associated resources to a connected client.
  class ClientData
  {
  public:
    ClientData(std::unique_ptr<Socket> Connection);
    ~ClientData();

    std::size_t id() const noexcept { return ID; }
    /// Returns the most recent random-generated nonce for this client.
    std::size_t nonce() const noexcept { return Nonce.value_or(0); }
    /// Creates a new random number for the client, and returns it.
    std::size_t makeNewNonce() noexcept;

    Socket& getControlSocket() noexcept { return *ControlConnection; }
    Socket* getDataSocket() noexcept { return DataConnection.get(); }

  private:
    std::size_t ID;
    std::optional<std::size_t> Nonce = 0;

    /// The control connection transcieves control information and commands.
    std::unique_ptr<Socket> ControlConnection;

    /// TODO: ?
    std::unique_ptr<Socket> DataConnection;
  };

  static std::string getServerSocketPath();

  /// Create a new server that will listen on the associated socket.
  Server(Socket&& Sock);

  ~Server();

  /// Start actively listening and handling connections.
  ///
  /// \note This is a blocking call!
  int listen();

private:
  Socket Sock;
  std::map<raw_fd, ClientData> Clients;

  std::atomic_bool TerminateListenLoop = ATOMIC_VAR_INIT(false);
  std::unique_ptr<EPoll> Poll;

  void acceptCallback(ClientData& Client);
  void readCallback(ClientData& Client);
  void exitCallback(ClientData& Client);

private:
  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<void(ClientData&, std::string_view)>>
    Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(ClientData& Client, std::string_view Message);
#include "Server.Dispatch.ipp"
};

} // namespace monomux
