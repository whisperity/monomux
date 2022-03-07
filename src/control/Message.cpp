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
#include "Message.hpp"

#include <cstring>

#define DECODE(NAME) std::optional<NAME> NAME::decode(std::string_view Buffer)
#define ENCODE(NAME) std::string NAME::encode(const NAME& Object)

namespace monomux
{

MessageBase kindFromStr(std::string_view Str) noexcept
{
  MessageBase MB;
  {
    char MKCh[sizeof(MessageKind)] = {0};
    for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
      MKCh[I] = Str[I];
    MB.Kind = *reinterpret_cast<MessageKind*>(MKCh);
  }
  Str.remove_prefix(sizeof(MessageKind));
  MB.RawData = Str;
  return MB;
}

std::string kindToStr(MessageKind MK) noexcept
{
  std::string Str;
  Str.resize(sizeof(MessageKind));

  char* Data = reinterpret_cast<char*>(&MK);
  for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
    Str[I] = Data[I];

  return Str;
}

/// Reads \p Literal from the \p Data and returns a new \p string_view that
/// points after the consumed \p Literal, or the empty \p string_view if it was
/// not found.
static std::string_view consume(std::string_view Data,
                                const std::string_view Literal)
{
  auto Pos = Data.find(Literal);
  if (Pos != 0)
    return {};
  Data.remove_prefix(Literal.size());
  return Data;
}

/// Returns the \p string_view into \p Data until the first occurrence of
/// \p Literal.
static std::string_view takeUntil(std::string_view Data,
                                  const std::string_view Literal)
{
  auto Pos = Data.find(Literal);
  if (Pos == std::string_view::npos)
    return {};
  return Data.substr(0, Pos);
}

namespace request
{

ENCODE(ClientID)
{
  (void)Object;
  return "<CLIENT-ID />";
}
DECODE(ClientID)
{
  if (Buffer == "<CLIENT-ID />")
    return ClientID{};
  return std::nullopt;
}

ENCODE(DataSocket)
{
  std::string Ret = "<DATASOCKET><CLIENT-ID>";
  Ret.append(std::to_string(Object.Client.ID));
  Ret.append("<NONCE>");
  Ret.append(std::to_string(Object.Client.Nonce));
  Ret.append("</NONCE>");
  Ret.append("</CLIENT-ID></DATASOCKET>");
  return Ret;
}
DECODE(DataSocket)
{
  DataSocket Ret;

  auto View = consume(Buffer, "<DATASOCKET><CLIENT-ID>");
  if (View.empty())
    return std::nullopt;

  auto ID = takeUntil(View, "<NONCE>");
  if (ID.empty())
    return std::nullopt;
  Ret.Client.ID = std::stoull(std::string{ID});
  View.remove_prefix(ID.size());

  View = consume(View, "<NONCE>");
  if (View.empty())
    return std::nullopt;

  auto Nonce = takeUntil(View, "</NONCE>");
  if (Nonce.empty())
    return std::nullopt;
  Ret.Client.Nonce = std::stoull(std::string{Nonce});
  View.remove_prefix(Nonce.size());

  View = consume(View, "</NONCE>");
  if (View.empty())
    return std::nullopt;

  if (View != "</CLIENT-ID></DATASOCKET>")
    return std::nullopt;

  return Ret;
}

ENCODE(SpawnProcess)
{
  std::string Ret = "<SPAWN>";
  Ret.append(Object.ProcessName);
  Ret.push_back('\0');
  Ret.append("</SPAWN>");
  return Ret;
}
DECODE(SpawnProcess)
{
  SpawnProcess Ret;

  auto P = Buffer.find("<SPAWN>");
  if (P == std::string::npos)
    return std::nullopt;
  P += std::strlen("<SPAWN>");

  auto SV = std::string_view{Buffer.data() + P, Buffer.size() - P};
  P = SV.find('\0');
  Ret.ProcessName = std::string{SV.substr(0, P)};
  SV.remove_prefix(P + 1);

  if (SV != "</SPAWN>")
    return std::nullopt;

  return Ret;
}

} // namespace request

namespace response
{

ENCODE(ClientID)
{
  std::string Ret = "<CLIENT-ID>";
  Ret.append(std::to_string(Object.Client.ID));
  Ret.append("<NONCE>");
  Ret.append(std::to_string(Object.Client.Nonce));
  Ret.append("</NONCE>");
  Ret.append("</CLIENT-ID>");
  return Ret;
}
DECODE(ClientID)
{
  ClientID Ret;

  auto View = consume(Buffer, "<CLIENT-ID>");
  if (View.empty())
    return std::nullopt;

  auto ID = takeUntil(View, "<NONCE>");
  if (ID.empty())
    return std::nullopt;
  Ret.Client.ID = std::stoull(std::string{ID});
  View.remove_prefix(ID.size());

  View = consume(View, "<NONCE>");
  if (View.empty())
    return std::nullopt;

  auto Nonce = takeUntil(View, "</NONCE>");
  if (Nonce.empty())
    return std::nullopt;
  Ret.Client.Nonce = std::stoull(std::string{Nonce});
  View.remove_prefix(Nonce.size());

  View = consume(View, "</NONCE>");
  if (View.empty())
    return std::nullopt;

  if (View != "</CLIENT-ID>")
    return std::nullopt;

  return Ret;
}

ENCODE(DataSocket)
{
  std::string Ret = "<DATASOCKET>";
  Ret.append(Object.Success ? "Accept" : "Deny");
  Ret.append("</DATASOCKET>");
  Ret.append("!MAINTAIN-RADIO-SILENCE!\0\0\0");
  return Ret;
}
DECODE(DataSocket)
{
  DataSocket Ret;

  auto View = consume(Buffer, "<DATASOCKET>");
  if (View.empty())
    return std::nullopt;

  auto Value = takeUntil(View, "</DATASOCKET>");
  if (Value.empty())
    return std::nullopt;
  if (Value == "Accept")
    Ret.Success = true;
  else if (Value == "Deny")
    Ret.Success = false;
  else
    return std::nullopt;
  View.remove_prefix(Value.size());

  View = consume(View, "</DATASOCKET>");
  if (View.empty())
    return std::nullopt;

  // Ignore the "radio silence" message. :)

  return Ret;
}

} // namespace response

} // namespace monomux

#undef ENCODE
#undef DECODE
