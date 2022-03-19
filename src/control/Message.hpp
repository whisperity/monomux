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
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace monomux
{
namespace message
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
/// network transmission.
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

/// A view of the \p Server::SessionData data structure that is sufficient for
/// network transmission.
struct SessionData
{
  MONOMUX_MESSAGE_BASE(SessionData);

  /// \see server::SessionData::Name.
  std::string Name;

  /// \see server::SessionData::Created.
  std::time_t Created;
};

/// A base class for responding boolean values consistently.
struct Boolean
{
  MONOMUX_MESSAGE_BASE(Boolean);

  operator bool() const { return Value; }
  Boolean& operator=(bool V)
  {
    Value = V;
    return *this;
  }

  bool Value;
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
  monomux::message::ClientID Client;
};

/// A request from the client to the server to advise the client about the
/// sessions available on the server for attachment.
struct SessionList
{
  MONOMUX_MESSAGE(SessionListRequest, SessionList);
};

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

/// A request from the client to the server to attach the client to the
/// specified session.
struct Attach
{
  MONOMUX_MESSAGE(AttachRequest, Attach);
  /// The name of the session to attach to.
  std::string Name;
};

} // namespace request

namespace response
{

/// The response to the \p request::ClientID, sent by the server.
struct ClientID
{
  MONOMUX_MESSAGE(ClientIDResponse, ClientID);
  monomux::message::ClientID Client;
};

/// The response to the \p request::DataSocket, sent by the server.
///
/// This message is sent back through the connection the request arrived.
/// In case of \p Success, this is the last (and only) control message that is
/// passed through what transmogrified into a \e Data connection.
struct DataSocket
{
  MONOMUX_MESSAGE(DataSocketResponse, DataSocket);
  monomux::message::Boolean Success;
};

/// The response to the \p request::SessionList, sent by the server.
struct SessionList
{
  MONOMUX_MESSAGE(SessionListResponse, SessionList);
  std::vector<monomux::message::SessionData> Sessions;
};

/// The response to the \p request::MakeSession,sent by the server.
struct MakeSession
{
  MONOMUX_MESSAGE(MakeSessionResponse, MakeSession);
  monomux::message::Boolean Success;

  /// The name of the created session. This \b MAY \b NOT be the same as the
  /// \e requested \p Name.
  std::string Name;
};

struct Attach
{
  MONOMUX_MESSAGE(AttachResponse, Attach);
  monomux::message::Boolean Success;
};

} // namespace response

} // namespace message
} // namespace monomux
