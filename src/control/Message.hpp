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

namespace monomux
{

namespace request
{

/// A request from the client to the server to deliver the identity information
/// to the client.
///
/// This message is sent as the initial handshake after a connection is
/// established.
struct ClientID
{
  MONOMUX_MESSAGE(REQ_ClientID, ClientID)
};

struct SpawnProcess
{
  MONOMUX_MESSAGE(REQ_SpawnProcess, SpawnProcess)
  std::string ProcessName;
};

} // namespace request

namespace response
{

/// The response to the \p request::ClientID, sent by the server.
struct ClientID
{
  MONOMUX_MESSAGE(RSP_ClientID, ClientID)
  /// The identity number of the client on the server it has connected to.
  std::size_t ID;
  /// A single-use number the client can use in other unassociated requests
  /// to prove its identity.
  std::size_t Nonce;
};

} // namespace response

} // namespace monomux
