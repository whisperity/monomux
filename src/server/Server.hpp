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
#include "ClientData.hpp"
#include "SessionData.hpp"

#include "adt/SmallIndexMap.hpp"
#include "adt/TaggedPointer.hpp"
#include "system/Socket.hpp"
#include "system/epoll.hpp"
#include "system/fd.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <variant>

namespace monomux
{

/// The monomux server is responsible for creating child processes of sessions.
/// Clients communicate with a \p Server instance to obtain information about
/// a session and to initiate attachment procedures.
class Server
{
public:
  static std::string getServerSocketPath();

  /// Create a new server that will listen on the associated socket.
  Server(Socket&& Sock);

  ~Server();

  /// Start actively listening and handling connections.
  ///
  /// \note This is a blocking call!
  int listen();

private:
  /// Create a data structure that allows us to (in the optimal case) quickly
  /// resolve a file descriptor to its origin kind, e.g. whether the connection
  /// is a client control connection, a client data connection, or a session
  /// connection.
  enum ConnectionTag
  {
    CT_None = 0,
    CT_ClientControl = 1,
    CT_ClientData = 2,
    CT_Session = 4
  };

  using ClientControlConnection = TaggedPointer<CT_ClientControl, ClientData>;
  using ClientDataConnection = TaggedPointer<CT_ClientData, ClientData>;
  using SessionConnection = TaggedPointer<CT_Session, SessionData>;
  using LookupVariant = std::variant<std::monostate,
                                     ClientControlConnection,
                                     ClientDataConnection,
                                     SessionConnection>;

private:
  Socket Sock;

  /// A quick lookup that associates a file descriptor to the data for the
  /// entity behind the file descriptor.
  SmallIndexMap<LookupVariant,
                256,
                /* StoreInPlace =*/true,
                /* IntrusiveDefaultSentinel =*/true>
    FDLookup;

  /// Map client IDs to the client information data structure.
  ///
  /// \note \p unique_ptr is used so changing the map's balancing does not
  /// invalidate other references to the data.
  std::map<std::size_t, std::unique_ptr<ClientData>> Clients;

  /// Map terminal \p Sessions running under the current shell to their names.
  ///
  /// \note \p unique_ptr is used so changing the map's balancing does not
  /// invalidate other references to the data.
  std::map<std::string, std::unique_ptr<SessionData>> Sessions;

  std::atomic_bool TerminateListenLoop = ATOMIC_VAR_INIT(false);
  std::unique_ptr<EPoll> Poll;

  /// The callback function that is fired when a new \p Client connected.
  void acceptCallback(ClientData& Client);
  /// The callback function that is fired for transmission on a \p Client's
  /// control connection.
  void controlCallback(ClientData& Client);
  /// The clalback function that is fired for transmission on a \p Client's
  /// data connection.
  void dataCallback(ClientData& Client);
  /// The callback function that is fired when a \p Client has disconnected.
  void exitCallback(ClientData& Client);

  /// The callback function that is fired when a new \p Session was created.
  void createCallback(SessionData& Session);
  /// The callback function that is fired when the server-side of a \p Session
  /// receives data.
  void dataCallback(SessionData& Session);
  /// The callback function that is fired when a \p Client attaches to a
  /// \p Session.
  void clientAttachedCallback(ClientData& Client, SessionData& Session);
  /// The callback function that is fired when a \p Client had detached from a
  /// \p Session.
  void clientDetachedCallback(ClientData& Client, SessionData& Session);
  /// The callback function that is fired when a \p Session is destroyed.
  void destroyCallback(SessionData& Session);

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
