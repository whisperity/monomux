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

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#ifndef MONOMUX_MESSAGE
#define MONOMUX_MESSAGE(KIND, NAME)                                            \
  static constexpr MessageKind Kind = MessageKind::KIND;                       \
  static std::optional<NAME> decode(std::string_view Buffer);                  \
  static std::string encode(const NAME& Object);
#endif

namespace monomux
{

class Socket;

/// A global enumeration table of messages that are supported by the protocol.
/// For each entry, an appropriate struct in namespace \p monomux::request or
/// \p monomux::response defines the data members of the message.
enum class MessageKind : std::uint16_t
{
  /// A request to the server to reply the client's ID to the client.
  REQ_ClientID = 1,
  /// A response for the \p REQ_ClientID request, containing the client's ID.
  RSP_ClientID = 2,
  /// A request to the server to associate the connection with another client,
  /// marking it as the data connection/socket.
  REQ_DataSocket = 3,
  /// A response for the \p REQ_DataSocket request, indicating whether the
  /// request was accepted.
  RSP_DataSocket = 4,

  /// ?
  REQ_SpawnProcess = 4000,
};

/// Helper class that contains the parsed \p MessageKind of a \p Message, and
/// the remaining, not yet parsed \p Buffer.
struct MessageBase
{
  MessageKind Kind;
  std::string_view RawData;
};

/// Unpack the \p MessageKind from the prefix of the \p Str.
MessageBase kindFromStr(std::string_view Str) noexcept;
/// Format the given \p MessageKind into a binary string.
std::string kindToStr(MessageKind MK) noexcept;

} // namespace monomux
