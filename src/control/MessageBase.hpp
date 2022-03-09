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
  /// Indicates a broken message that failed to read as a proper entity.
  Invalid = 0,

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

  /// Encodes the \p Kind variable as a binary string for appropriately
  /// prefixing a transmissible buffer.
  std::string encodeKind() const;

  /// Decodes the binary prefix of a message as a \p Kind.
  static MessageKind decodeKind(std::string_view Str) noexcept;

  /// Pack a raw and encoded message into a full transmissible buffer.
  std::string pack() const;

  /// Unpack an encoded and fully read message into its base constitutents.
  static MessageBase unpack(std::string_view Str) noexcept;
};

/// Encodes a message object into its raw data form.
template <typename T> std::string encode(const T& Msg)
{
  std::string RawForm = T::encode(Msg);

  MessageBase MB;
  MB.Kind = Msg.Kind;
  MB.RawData = RawForm;

  return MB.pack();
}

/// Decodes the given received buffer as a specific message object, and returns
/// it if successful.
template <typename T> std::optional<T> decode(std::string_view Str) noexcept
{
  MessageBase MB = MessageBase::unpack(Str);
  if (MB.Kind == MessageKind::Invalid)
    return std::nullopt;

  std::optional<T> Msg = T::decode(MB.RawData);
  return Msg;
}

} // namespace monomux
