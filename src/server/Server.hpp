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
#include "Session.hpp"

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
  /// Map client IDs to the client information data structure. \p unique_ptr
  /// is used so changing the map's balancing does not invalidate other
  /// references to the data.
  std::map<std::size_t, std::unique_ptr<ClientData>> Clients;
  /// Map data connection file descriptors to the client that owns the data
  /// connection.
  std::map<raw_fd, ClientData*> DataConnections;
  std::map<std::string, Session> Sessions;

  std::atomic_bool TerminateListenLoop = ATOMIC_VAR_INIT(false);
  std::unique_ptr<EPoll> Poll;

  void acceptCallback(ClientData& Client);
  void controlCallback(ClientData& Client);
  void dataCallback(ClientData& Client);
  void exitCallback(ClientData& Client);

  /// A special step during the handshake maneuvre is when a user client
  /// connects to the server again, and establishes itself as the data
  /// connection of its own already existing control client.
  ///
  /// This method takes care of associating that in the \p Clients map.
  void turnClientIntoDataOfOtherClient(ClientData& MainClient,
                                       ClientData& DataClient);

private:
  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<void(ClientData&, std::string_view)>>
    Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(ClientData& Client, std::string_view Message);
#include "Dispatch.ipp"
};

} // namespace monomux
