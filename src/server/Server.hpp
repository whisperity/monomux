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
#include "system/EPoll.hpp"
#include "system/Process.hpp"
#include "system/Socket.hpp"
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
namespace server
{

/// The monomux server is responsible for creating child processes of sessions.
/// Clients communicate with a \p Server instance to obtain information about
/// a session and to initiate attachment procedures, and then it is the
/// \p Server which transcieves data back and forth in a connection.
///
/// The conventional way of executing a Monomux Server is by letting the
/// \p listen() call deal with the control messages and structures, coming from
/// an "official" Monomux Client. However, the callbacks are exposed to users
/// who might want to embed Monomux as a library and self-setup some
/// connections.
///
/// The server exposes several \e callback functions which can be used to
/// manipulate the internal data structures appropriately.
/// Handling of incoming messages can be overridden by installing one's own
/// dispatch routines.
///
/// \note Some functionality of the server process (e.g. spawning and reaping
/// subprocesses) requires proper signal handling, which the \p Server does
/// \b NOT implement internally! It is up to the program embedding the server to
/// construct and set up appropriate handlers!
class Server
{
public:
  /// The type of message handler functions.
  ///
  /// \param Client The server-side client data structure for the entity that
  /// sent the message.
  /// \param RawMessage A view into the buffer of the message, before any
  /// structural parsing had been applied.
  using HandlerFunction = void(ClientData& Client, std::string_view RawMessage);

  /// Create a new server that will listen on the associated socket.
  Server(Socket&& Sock);

  ~Server();

  /// Override the default handling logic for the specified message \p Kind to
  /// fire the user-given \p Handler \b instead \b of the built-in default.
  void registerMessageHandler(std::uint16_t Kind,
                              std::function<HandlerFunction> Handler);

  /// Start actively listening and handling connections.
  ///
  /// \note This is a blocking call!
  void loop();

  /// Atomcially request the server's \p listen() loop to die.
  void interrupt() const noexcept;

  /// After the server's \p listen() loop has terminated, performs graceful
  /// shutdown of connections and sessions.
  void shutdown();

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

  Socket Sock;

  static constexpr std::size_t FDLookupSize = 128;
  /// A quick lookup that associates a file descriptor to the data for the
  /// entity behind the file descriptor.
  SmallIndexMap<LookupVariant,
                FDLookupSize,
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

  static constexpr std::size_t DeadChildrenVecSize = 8;
  /// A list of process handles that were signalle
  mutable std::array<Process::raw_handle, DeadChildrenVecSize> DeadChildren;

  mutable std::atomic_bool TerminateListenLoop = ATOMIC_VAR_INIT(false);
  std::unique_ptr<EPoll> Poll;

  void reapDeadChildren();
  /// Sends a connection accpetance message to the client.
  void sendAcceptClient(ClientData& Client);
  /// Sends a rejection message to the client.
  void sendRejectClient(ClientData& Client, std::string Reason);

public:
  /// Retrieve data about the client registered as \p ID.
  ClientData* getClient(std::size_t ID) noexcept;

  /// Retrieve data about the session registered as \p Name.
  SessionData* getSession(std::string_view Name) noexcept;

  /// Creates a new client on the server.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks!
  ClientData* makeClient(ClientData Client);

  /// Regiters a new session to the server.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks!
  SessionData* makeSession(SessionData Session);

  /// Delete the \p Client from the list of clients.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks! Importantly, the connection
  /// streams are not closed gracefully by this call.
  void removeClient(ClientData& Client);

  /// Delete the \p Session from the list of sessions.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks! Importantly, the connection
  /// streams and resources of the session are not shut down gracefully by this
  /// call.
  void removeSession(SessionData& Session);

  /// Adds the specified \p PID to the list of subprocesses of the server that
  /// had died. This function is meaningful to be called from a signal handler.
  /// The server's \p loop() will take care of destroying the session in its
  /// normal iteration.
  void registerDeadChild(Process::raw_handle PID) const noexcept;

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
  std::map<std::uint16_t, std::function<HandlerFunction>> Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(ClientData& Client, std::string_view Message);
#include "Dispatch.ipp"
};

} // namespace server
} // namespace monomux
