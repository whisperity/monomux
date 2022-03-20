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
#include "SessionData.hpp"
#include "Terminal.hpp"

#include "adt/MovableAtomic.hpp"
#include "adt/unique_scalar.hpp"
#include "system/EPoll.hpp"
#include "system/Process.hpp"
#include "system/Socket.hpp"

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace monomux
{
namespace client
{

/// This class represents a connection to a running \p Server - or rather,
/// a wrapper over the communication channel that allows talking to the
/// server.
///
/// In networking terminology, this is effectively a client, but we reserve
/// that word for the "Monomux Client" which deals with attaching to a
/// process.
///
/// \note Some functionality of the client instance (e.g. sending signals over
/// to the attached session) requires proper signal handling, which the
/// \p Client does \b NOT implement internally! It is up to the program
/// embedding the client to construct and set up appropriate handlers!
class Client
{
public:
  enum ExitReason
  {
    None = 0,
    /// The client terminated because of internal logic failure. This is an
    /// \b error condition.
    Failed,
    /// The client was terminated by the user via a kill signal.
    Terminated,
    /// The client was terminated because the controlling terminal hung up.
    Hangup,
    /// The client exited because the server disconnected it.
    Detached,
    /// The client exited because the attached session exited.
    SessionExit,
    /// The client exit because the server shut down.
    ServerExit,
  };

  /// The type of message handler functions.
  ///
  /// \param Client The \p Client manager that received the message.
  /// \param RawMessage A view into the buffer of the message, before any
  /// structural parsing had been applied.
  using HandlerFunction = void(Client& Client, std::string_view RawMessage);

  /// Creates a new connection client to the server at the specified socket.
  ///
  /// \param RejectReason If specified, and the server gracefully rejected the
  /// connection, the reason for the rejection (as reported by the server) will
  /// be written to.
  static std::optional<Client> create(std::string SocketPath,
                                      std::string* RejectReason);

  /// Initialise a \p Client over the already established \p ControlSocket.
  Client(Socket&& ControlSocket);

  /// Override the default handling logic for the specified message \p Kind to
  /// fire the user-given \p Handler \b instead \b of the built-in default.
  void registerMessageHandler(std::uint16_t Kind,
                              std::function<HandlerFunction> Handler);

  Socket& getControlSocket() noexcept { return ControlSocket; }
  const Socket& getControlSocket() const noexcept { return ControlSocket; }

  Terminal* getTerminal() noexcept { return Term ? &*Term : nullptr; }
  const Terminal* getTerminal() const noexcept
  {
    return Term ? &*Term : nullptr;
  }

  /// Sets the \p Client to be attached to the streams of the \p T terminal.
  void setTerminal(Terminal&& T);

  Socket* getDataSocket() noexcept
  {
    return DataSocket ? DataSocket.get() : nullptr;
  }
  const Socket* getDataSocket() const noexcept
  {
    return DataSocket ? DataSocket.get() : nullptr;
  }
  /// Takes ownership of and stores the given \p Socket as the data socket of
  /// the client.
  ///
  /// \note No appropriate handshaking is done by this call! The server needs to
  /// be communicated with in advance to associate the connection with the
  /// client.
  void setDataSocket(Socket&& DataSocket);

  /// Perform a handshake mechanism over the control socket.
  ///
  /// A successful handshake initialises the client to be fully \e capable of
  /// both control and data communication with the server, but does not start
  /// the handling logic (see \p loop()).
  ///
  /// \param FailureReason If set and the handshake procedure fails, a
  /// human-readable reason for it will be written.
  ///
  /// \return Whether the handshake process succeeded.
  bool handshake(std::string* FailureReason);

  /// Starts the main loop of the client, taking control of the terminal and
  /// actually communicating data with the server.
  void loop();

  ExitReason exitReason() const noexcept { return Exit; }

  /// Sends a request to the connected server to tell what sessions are running
  /// on the server.
  ///
  /// \returns The data received from the server, or \p nullopt, if
  /// commmuniation failed.
  std::optional<std::vector<SessionData>> requestSessionList();

  /// Sends a request of new session creation to the server the client is
  /// connected to.
  ///
  /// \param Name The name to associate with the session. This is non-normative,
  /// and the server may overrule the request.
  /// \param Opts Details of the process to spawn on the server's end.
  ///
  /// \returns The actual name of the created session, if creation was
  /// successful.
  std::optional<std::string> requestMakeSession(std::string Name,
                                                Process::SpawnOptions Opts);

  /// Sends a request to the server to attach the client to the session
  /// identified by \p SessionName.
  ///
  /// \return whether the attachment succeeded.
  bool requestAttach(std::string SessionName);

  /// \returns whether the client successfully attached to a session on the
  /// server.
  ///
  /// \see requestAttach()
  bool attached() const noexcept { return Attached; }

  /// \returns information about the session the client is (if \p attached() is
  /// \p true) or last was (if \p attached() is \p false) attached to. If the
  /// client never attached to any session, returns \p nullptr.
  const SessionData* attachedSession() const noexcept
  {
    return AttachedSession ? &*AttachedSession : nullptr;
  }

  /// Sends \p Data to the server over the \e data connection.
  void sendData(std::string_view Data);

  /// The callback that is fired when data is available on the \e control
  /// connection of the client.
  void controlCallback();

  /// Handle data received on the \e data connection.
  void dataCallback();

  /// Handle data received as an \e input to the client.
  void inputCallback();

private:
  /// The control socket is used to communicate control commands with the
  /// server.
  Socket ControlSocket;

  /// The data connection is used to transmit the process data to the client.
  /// (This is initialised in a lazy fashion during operation.)
  std::unique_ptr<Socket> DataSocket;

  /// Whether continuous \e handling of data on the \p DataSocket (if connected)
  /// via \p Poll is enabled.
  unique_scalar<bool, false> DataSocketEnabled;

  /// Whether the client successfully attached to a session on the server.
  unique_scalar<bool, false> Attached;

  /// Information about the session the client attached to.
  std::optional<SessionData> AttachedSession;

  /// The terminal the \p Client is attached to, if any.
  std::optional<Terminal> Term;

  unique_scalar<ExitReason, None> Exit;
  /// Terminate the handling \p loop() of the client and set the exit status to
  /// \p E.
  void exit(ExitReason E);

  mutable MovableAtomic<bool> TerminateLoop = false;
  std::unique_ptr<EPoll> Poll;
  /// Initialises (or resets) the \p Poll event polling low-level structures.
  void setupPoll();
  /// If channel polling is initialised, adds \p DataSocket to the list of
  /// channels to poll and handle incoming data.
  void enableDataSocket();
  /// If channel polling is initialised, removes \p DataSocket from the list of
  /// channels to poll. When disabled, data sent by the server is left
  /// unhandled.
  void disableDataSocket();
  /// If channel polling is initialised, adds \p ControlSocket to the list of
  /// channels to poll and handle incoming messages.
  void enableControlResponsePoll();
  /// If channel polling is initialised, removes \p ControlSocket from the list
  /// of channels to poll. When inhibited, messages sent by the server are
  /// expected to be handled synchronously by the request sending function,
  /// instead of being handled "automatically" by a dispatch handler.
  void inhibitControlResponsePoll();

  friend class ControlPollInhibitor;
  /// Inhibits the poll handler from handling responses on the \e control
  /// connection in the current scope.
  class ControlPollInhibitor
  {
    Client& C;

  public:
    ControlPollInhibitor(Client& C);
    ~ControlPollInhibitor();
  };
  ControlPollInhibitor scopedInhibitControlResponsePoll();

  /// A unique identifier of the current \p Client, as returned by the server.
  std::size_t ClientID = -1;

  /// A unique, random generated single-use number, which the \p Client can
  /// use to establish its identity towards the server in another request.
  std::optional<std::size_t> Nonce;

  /// Return the stored \p Nonce of the current instance, resetting it.
  std::size_t consumeNonce() noexcept;

  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<HandlerFunction>> Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  static void FUNCTION_NAME(Client& Client, std::string_view Message);
#include "Dispatch.ipp"
};

} // namespace client
} // namespace monomux
