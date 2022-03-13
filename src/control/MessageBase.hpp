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

#ifndef MONOMUX_MESSAGE_BASE
#define MONOMUX_MESSAGE_BASE(NAME)                                             \
  static constexpr MessageKind Kind = MessageKind::Base;                       \
  static std::optional<NAME> decode(std::string_view& Buffer);                 \
  static std::string encode(const NAME& Object);
#endif

namespace monomux
{

/// A global enumeration table of messages that are supported by the protocol.
/// For each entry, an appropriate struct in namespace \p monomux::request or
/// \p monomux::response defines the data members of the message.
enum class MessageKind : std::uint16_t
{
  /// Indicates a broken message that failed to read as a proper entity.
  Invalid = 0,
  /// Indicates a subobject of a \p Message that cannot be understood
  /// individually.
  Base = static_cast<std::uint16_t>(-1),

  // Handskahe procedure messages.

  /// A request to the server to reply the client's ID to the client.
  ClientIDRequest = 0x0001,
  /// A response for the \p ClientIDRequest, containing the client's ID.
  ClientIDResponse = 0x0002,

  /// A request to the server to associate the connection with another client,
  /// marking it as the data connection/socket.
  DataSocketRequest = 0x0003,
  /// A response for the \p DataSocketRequest, indicating whether the
  /// request was accepted.
  DataSocketResponse = 0x0004,

  // Session management messages.

  /// A request to the server to reply with data about the running sessions on
  /// the server.
  SessionListRequest = 0x0101,
  /// A response to the \p SessionListRequest, containing \p Session data.
  SessionListResponse = 0x0102,

  /// A request to the server to create a brand new new session.
  MakeSessionRequest = 0x0103,
  /// A reponse to the \p MakeSessionRequest containing the results of the new
  /// session.
  MakeSessionResponse = 0x0104,
};

/// Helper class that contains the parsed \p MessageKind of a \p Message, and
/// the remaining, not yet parsed \p Buffer.
struct MessageBase
{
  MessageKind Kind;
  std::string_view RawData;

  /// Encodes the given number as a platform-specific binary-string.
  static std::string sizeToBinaryString(std::size_t N);

  /// Decodes a \p std::size_t from the given \p Str.
  static std::size_t binaryStringToSize(std::string_view Str) noexcept;

  /// Encodes the \p Kind variable as a binary string for appropriately
  /// prefixing a transmissible buffer.
  std::string encodeKind() const;

  /// Decodes the binary prefix of a message as a \p Kind.
  static MessageKind decodeKind(std::string_view Str) noexcept;

  /// Pack a raw and encoded message into a full transmissible payload.
  std::string pack() const;

  /// Unpack an encoded and fully read payload into its base constitutents.
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

/// Encodes a message object into its raw data form, prefixed with a payload
/// size.
template <typename T> std::string encodeWithSize(const T& Msg)
{
  std::string Payload = encode(Msg);
  return MessageBase::sizeToBinaryString(Payload.size()) + std::move(Payload);
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
