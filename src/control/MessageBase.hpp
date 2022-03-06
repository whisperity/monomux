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
  /// A response containing the client's ID.
  RSP_ClientID = 2,
  /// ?
  REQ_DataSocket = 3,

  /// ?
  REQ_SpawnProcess = 4000,
};

inline MessageKind kindFromStr(const std::string& Str) noexcept
{
  MessageKind MK;
  {
    char MKCh[sizeof(MessageKind)] = {0};
    for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
      MKCh[I] = Str[I];
    MK = *reinterpret_cast<MessageKind*>(MKCh);
  }
  return MK;
}

inline std::string kindToStr(MessageKind MK) noexcept
{
  std::string Str;
  Str.resize(sizeof(MessageKind));

  char* Data = reinterpret_cast<char*>(&MK);
  for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
    Str[I] = Data[I];

  return Str;
}

} // namespace monomux
