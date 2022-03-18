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
class Client
{
public:
  /// Creates a new connection client to the server at the specified socket.
  static std::optional<Client> create(std::string SocketPath);

  /// Initialise a \p Client over the already established \p ControlSocket.
  Client(Socket&& ControlSocket);

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
  /// \return Whether the handshake process succeeded.
  bool handshake();

  /// Starts the main loop of the client, taking control of the terminal and
  /// communicating with the server.
  void loop();

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
  /// \returns Whether the creation of the session was successful, as reported
  /// by the server.
  bool requestMakeSession(std::string Name, Process::SpawnOptions Opts);

  /// Sends \p Data to the server over the \e data connection.
  void sendData(std::string_view Data);

private:
  /// The control socket is used to communicate control commands with the
  /// server.
  Socket ControlSocket;

  /// The data connection is used to transmit the process data to the client.
  /// (This is initialised in a lazy fashion during operation.)
  std::unique_ptr<Socket> DataSocket;

  /// The terminal the \p Client is attached to, if any.
  std::optional<Terminal> Term;

  MovableAtomic<bool> TerminateLoop = false;
  std::unique_ptr<EPoll> Poll;
  void setupPoll();
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

private:
  /// Maps \p MessageKind to handler functions.
  std::map<std::uint16_t, std::function<void(std::string_view)>> Dispatch;

  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(std::string_view Message);
#include "Dispatch.ipp"
};

} // namespace client
} // namespace monomux
