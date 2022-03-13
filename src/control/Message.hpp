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
#include "MessageBase.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace monomux
{

/// Contains the data members required to identify a connected Client.
struct ClientID
{
  MONOMUX_MESSAGE_BASE(ClientID);

  /// The identity number of the client on the server it has connected to.
  std::size_t ID;
  /// A single-use number the client can use in other unassociated requests
  /// to prove its identity.
  std::size_t Nonce;
};

/// A view of the \p Process::SpawnOptions data structure that is sufficient for
/// network transmission as a request.
struct ProcessSpawnOptions
{
  MONOMUX_MESSAGE_BASE(ProcessSpawnOptions);

  /// \see Process::SpawnOptions::Program
  std::string Program;
  /// \see Process::SpawnOptions::Arguments
  std::vector<std::string> Arguments;

  // (We do not wish to deal with std::nullopt stuff...)

  /// The list of environment variables to be set for the spawned process.
  ///
  /// \see Process::SpawnOptions::Environment
  std::vector<std::pair<std::string, std::string>> SetEnvironment;

  /// The list of environment variables to be ignored and unset in the spawned
  /// process, no matter what the server has them set to.
  ///
  /// \see Process::SpawnOptions::Environment
  std::vector<std::string> UnsetEnvironment;
};

namespace request
{

/// A request from the client to the server to deliver the identity information
/// to the client.
///
/// This message is sent as the initial handshake after a connection is
/// established.
struct ClientID
{
  MONOMUX_MESSAGE(ClientIDRequest, ClientID);
};

/// A request from the client to the server sent over the data connection to
/// tell the server to register the connection this request is receieved on
/// to be the data connection of a connected \p Client.
///
/// This message is sent as the initial handshake after a connection is
/// established.
struct DataSocket
{
  MONOMUX_MESSAGE(DataSocketRequest, DataSocket);
  monomux::ClientID Client;
};

/// A request from the client to the server to advise the client about the
/// sessions available on the server for attachment.
// struct SessionList
// {
//   MONOMUX_MESSAGE(SessionListRequest, SessionList);
// };

/// A request from the client to the server to initialise a new session with
/// the specified parameters.
struct MakeSession
{
  MONOMUX_MESSAGE(MakeSessionRequest, MakeSession);
  /// The name to associate with the created session.
  ///
  /// \note This is non-normative, and may be rejected by the server.
  std::string Name;

  /// The options for the program to create in the session.
  ProcessSpawnOptions SpawnOpts;
};

} // namespace request

namespace response
{

/// The response to the \p request::ClientID, sent by the server.
struct ClientID
{
  MONOMUX_MESSAGE(ClientIDResponse, ClientID);
  monomux::ClientID Client;
};

/// The response to the \p request::DataSocket, sent by the server.
///
/// This message is sent back through the connection the request arrived.
/// In case of \p Success, this is the last (and only) control message that is
/// passed through what transmogrified into a \e Data connection.
struct DataSocket
{
  MONOMUX_MESSAGE(DataSocketResponse, DataSocket);
  bool Success;
};

/// The response to the \p request::SessionList, sent by the server.
// struct SessionList
// {
//   MONOMUX_MESSAGE(SessionListResponse, SessionList);
// };

/// The response to the \p request::MakeSession,sent by the server.
struct MakeSession
{
  MONOMUX_MESSAGE(MakeSessionResponse, MakeSession);
};

} // namespace response

} // namespace monomux
