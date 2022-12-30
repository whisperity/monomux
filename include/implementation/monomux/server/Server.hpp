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
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <variant>

#include "monomux/Config.h"
#include "monomux/adt/Atomic.hpp"
#include "monomux/adt/SmallIndexMap.hpp"
#include "monomux/adt/Tagged.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/IOEvent.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/Socket.hpp"

#include "ClientData.hpp"
#include "SessionData.hpp"

namespace monomux::server
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
  /// \param Server The \p Server manager that received the message.
  /// \param Client The server-side client data structure for the entity that
  /// sent the message.
  /// \param RawMessage A view into the buffer of the message, before any
  /// structural parsing had been applied.
  using HandlerFunction = void(Server& Server,
                               ClientData& Client,
                               std::string_view RawMessage);

#define MONOMUX_SERVER_HANDLER_SIGNATURE(NAME)                                 \
  void NAME(Server& Server, ClientData& Client, std::string_view Message)
/// When used inside a handler function, decodes the message identified by
/// \p MESSAGE_TYPE and makes it automatically available in the local \p Msg
/// variable.
#define MONOMUX_SERVER_HANDLER_DECODE(MESSAGE_TYPE)                            \
  using namespace monomux::message;                                            \
  std::optional<MESSAGE_TYPE> Msg = MESSAGE_TYPE::decode(Message);             \
  if (!Msg)                                                                    \
    return;                                                                    \
  MONOMUX_TRACE_LOG(LOG(trace) << __PRETTY_FUNCTION__);

  /// Create a new server that will listen on the associated socket.
  Server(std::unique_ptr<system::Socket>&& Sock);

  ~Server();

  [[nodiscard]] std::chrono::time_point<std::chrono::system_clock>
  whenStarted() const noexcept
  {
    return WhenStarted;
  }

#if MONOMUX_EMBEDDING_LIBRARY_FEATURES
  /// Must be thrown by the pre-handling callback to prevent the message from
  /// being handled by the main handler.
  class HandlingPreventingException : public std::runtime_error
  {
  public:
    HandlingPreventingException()
      : std::runtime_error("Handling of message should have been prevented.")
    {}

    [[nodiscard]] const char* what() const noexcept override
    {
      return std::runtime_error::what();
    }
  };

  /// Register \p Handler to be fired \b before the main handler is fired.
  /// Both handlers will receive a read-only view of the received message.
  ///
  /// \note Throw \p HandlingPreventingException in the custom handler if the
  /// main handling must be prevented, for some reason.
  void registerPreMessageHandler(std::uint16_t Kind,
                                 std::function<HandlerFunction> Handler);

  /// Register \p Handler to be fired \b after the main handler has
  /// successfully fired.
  /// Both handlers will receive a read-only view of the received message.
  void registerPostMessageHandler(std::uint16_t Kind,
                                  std::function<HandlerFunction> Handler);
#endif /* MONOMUX_EMBEDDING_LIBRARY_FEATURES */

  /// Sets whether the server should automatically close a \p loop() if the last
  /// session running under it terminated.
  void setExitIfNoMoreSessions(bool ExitIfNoMoreSessions);

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

  using ClientControlConnection = Tagged<CT_ClientControl, ClientData>;
  using ClientDataConnection = Tagged<CT_ClientData, ClientData>;
  using SessionConnection = Tagged<CT_Session, SessionData>;
  using LookupVariant = std::variant<std::monostate,
                                     ClientControlConnection,
                                     ClientDataConnection,
                                     SessionConnection>;

  std::unique_ptr<system::Socket> Sock;
  std::chrono::time_point<std::chrono::system_clock> WhenStarted;

  static constexpr std::size_t FDLookupSize = 256;
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
  /// A list of process handles that were signalled to be dead.
  mutable std::array<system::Process::Raw, DeadChildrenVecSize> DeadChildren;

  mutable Atomic<bool> TerminateLoop;
  bool ExitIfNoMoreSessions;
  std::unique_ptr<system::IOEvent> Poll;

  void reapDeadChildren();
  /// Sends a connection accpetance message to the client.
  void sendAcceptClient(ClientData& Client);
  /// Sends a rejection message to the client.
  void sendRejectClient(ClientData& Client, std::string Reason);

public:
  /// Retrieve data about the client registered as \p ID.
  [[nodiscard]] ClientData* getClient(std::size_t ID) noexcept;

  /// Retrieve data about the session registered as \p Name.
  [[nodiscard]] SessionData* getSession(std::string_view Name) noexcept;

  /// Creates a new client on the server.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks!
  [[nodiscard]] ClientData* makeClient(ClientData Client);

  /// Regiters a new session to the server.
  ///
  /// \note Calling this function only manages the backing data structure and
  /// does \b NOT fire any associated callbacks!
  [[nodiscard]] SessionData* makeSession(SessionData Session);

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
  void registerDeadChild(system::Process::Raw PID) const noexcept;

  /// Registers the new connected \p Client.
  void clientCreate(ClientData& Client);
  /// Destroys the data structures associated with a \p Client that
  /// disconnected.
  void clientExit(ClientData& Client);

  /// Registers the new created \p Session.
  void sessionCreate(SessionData& Session);
  /// Destroys the data structures associated with a \p Session that exited.
  void sessionDestroy(SessionData& Session);

  /// Registers that \p Client attached to \p Session.
  void clientAttached(ClientData& Client, SessionData& Session);
  /// Registers that \p Client detached from \p Session.
  void clientDetached(ClientData& Client, SessionData& Session);

  /// A special step during the handshake maneuvre is when a user client
  /// connects to the server again, and establishes itself as the data
  /// connection of its own already existing control client.
  ///
  /// This method takes care of associating that in the \p Clients map.
  void turnClientIntoDataOfOtherClient(ClientData& MainClient,
                                       ClientData& DataClient);

  /// \returns a statistical breakdown of the state of the server and the
  /// connections handled. This data is \b NOT meant to be machine-readable!
  [[nodiscard]] std::string statistics() const;

private:
  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<HandlerFunction>> MainDispatch;
#if MONOMUX_EMBEDDING_LIBRARY_FEATURES
  /// Maps \p MessageKind to handler functions that are executed \b before
  /// \p MainDispatch.
  decltype(MainDispatch) PreDispatch;
  /// Maps \p MessageKind to handler functions that are executed \b after
  /// \p MainDispatch.
  decltype(MainDispatch) PostDispatch;
#endif

  void setUpMainDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  static void FUNCTION_NAME(                                                   \
    Server& Server, ClientData& Client, std::string_view Message);
#include "monomux/server/Dispatch.ipp"

  /// The function responsible for understanding transmission on a \p Client's
  /// control connection. This method deals with parsing a \p Message
  /// from the control connection, and fire a message-specific handler.
  ///
  /// \see registerMessageHandler().
  void dispatchControl(ClientData& Client);
  /// The function responsible for understanding transmission on a \p Client's
  /// data connection. It sends the data received to the session the client
  /// attached to.
  void dispatchData(ClientData& Client);

  /// The function reponsible for understanding transmission on the server-side
  /// of a \p Session when receiving data. It sends the data received from the
  /// session to all attached clients.
  void dataCallback(SessionData& Session);
};

} // namespace monomux::server
