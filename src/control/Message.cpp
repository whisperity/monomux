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
#include "Messaging.hpp"
#include "system/unreachable.hpp"

#include <cstring>
#include <sstream>
#include <tuple>

#define DECODE(NAME) std::optional<NAME> NAME::decode(std::string_view Buffer)
#define ENCODE(NAME) std::string NAME::encode(const NAME& Object)

#define DECODE_BASE(NAME)                                                      \
  std::optional<NAME> NAME::decode(std::string_view& Buffer)
#define ENCODE_BASE(NAME) std::string NAME::encode(const NAME& Object)

namespace monomux
{
namespace message
{

std::string Message::sizeToBinaryString(std::size_t N)
{
  std::string Str;
  Str.resize(sizeof(std::size_t));

  const auto* Data = reinterpret_cast<const char*>(&N);
  for (std::size_t I = 0; I < sizeof(std::size_t); ++I)
    Str[I] = Data[I];

  return Str;
}

std::size_t Message::binaryStringToSize(std::string_view Str) noexcept
{
  if (Str.size() < sizeof(std::size_t))
    return 0;

  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  char SCh[sizeof(std::size_t)] = {0};
  for (std::size_t I = 0; I < sizeof(std::size_t); ++I)
    SCh[I] = Str[I];
  return *reinterpret_cast<std::size_t*>(SCh);
}

std::string Message::encodeKind() const
{
  std::string Str;
  Str.resize(sizeof(MessageKind));

  const auto* Data = reinterpret_cast<const char*>(&Kind);
  for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
    Str[I] = Data[I];

  return Str;
}

MessageKind Message::decodeKind(std::string_view Str) noexcept
{
  if (Str.size() < sizeof(MessageKind))
    return MessageKind::Invalid;

  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  char MKCh[sizeof(MessageKind)] = {0};
  for (std::size_t I = 0; I < sizeof(MessageKind); ++I)
    MKCh[I] = Str[I];
  return *reinterpret_cast<MessageKind*>(MKCh);
}

std::string Message::pack() const
{
  std::string Str;
  Str.reserve(sizeof(MessageKind) + RawData.size() + sizeof('\0'));

  Str.append(encodeKind());
  Str.append(RawData);
  Str.push_back('\0');

  return Str;
}

Message Message::unpack(std::string_view Str) noexcept
{
  Message MB;
  MB.Kind = decodeKind(Str);
  if (MB.Kind == MessageKind::Invalid)
    return MB;

  Str.remove_prefix(sizeof(MessageKind));
  if (Str.back() != '\0')
    return MB;
  Str.remove_suffix(sizeof('\0'));

  MB.RawData = Str;
  return MB;
}

std::string readPascalString(CommunicationChannel& Channel)
{
  std::string SizeStr = Channel.read(sizeof(std::size_t));
  std::size_t Size = Message::binaryStringToSize(SizeStr);
  std::string DataStr = Channel.read(Size);
  return DataStr;
}

namespace
{

/// Reads \p Literal from the \b beginning \p Data and returns a new
/// \p string_view that points after the consumed \p Literal, or the empty
/// \p string_view if it was not found.
std::string_view consume(std::string_view Data, const std::string_view Literal)
{
  auto Pos = Data.find(Literal);
  if (Pos != 0)
    return {};
  Data.remove_prefix(Literal.size());
  return Data;
}

/// Returns the \p string_view into \p Data until the first occurrence of
/// \p Literal.
std::string_view takeUntil(std::string_view Data,
                           const std::string_view Literal)
{
  auto Pos = Data.find(Literal);
  if (Pos == std::string_view::npos)
    return {};
  return Data.substr(0, Pos);
}

/// Returns \p N characters spliced from the beginning of \p Data and modifies
/// \p Data to point after the removed characters.
std::string_view splice(std::string_view& Data, std::size_t N)
{
  std::string_view S = Data.substr(0, N);
  Data.remove_prefix(N);
  return S;
}

/// Returns both the data taken until the \p Literal encountered, and the
/// remaining \p Data buffer. The \p Literal is \b NOT part of the returned
/// match.
std::string_view takeUntilAndConsume(std::string_view& Data,
                                     const std::string_view Literal)
{
  auto Pos = Data.find(Literal);
  if (Pos == std::string_view::npos)
    return {};

  std::string_view Match = Data.substr(0, Pos);
  Data.remove_prefix(Match.size() + Literal.size());
  return Match;
}

} // namespace

#define CONSUME_OR_NONE(LITERAL)                                               \
  View = consume(View, LITERAL);                                               \
  if (View.empty())                                                            \
    return std::nullopt;

#define PEEK_AND_CONSUME(LITERAL)                                              \
  if (std::string_view::size_type P = View.find(LITERAL);                      \
      P == 0 && (View.remove_prefix(std::strlen(LITERAL)), P == 0))

#define EXTRACT_OR_NONE(VARIABLE, UNTIL_LITERAL)                               \
  auto VARIABLE = takeUntilAndConsume(View, UNTIL_LITERAL);                    \
  if ((VARIABLE).empty())                                                      \
    return std::nullopt;

#define HEADER_OR_NONE(LITERAL)                                                \
  std::string_view View = consume(Buffer, LITERAL);                            \
  if (View.empty())                                                            \
    return std::nullopt;

// Must not use FOOTER_OR_NONE in "Base classes" because the decode buffer might
// contain additional data!
#define BASE_FOOTER_OR_NONE(LITERAL)                                           \
  CONSUME_OR_NONE(LITERAL)                                                     \
  Buffer = View;

ENCODE_BASE(ClientID)
{
  std::ostringstream Ret;
  Ret << "<CLIENT>";
  {
    Ret << "<ID>" << Object.ID << "</ID>";
    Ret << "<NONCE>" << Object.Nonce << "</NONCE>";
  }
  Ret << "</CLIENT>";
  return Ret.str();
}
DECODE_BASE(ClientID)
{
  ClientID Ret;
  HEADER_OR_NONE("<CLIENT>");

  CONSUME_OR_NONE("<ID>");
  EXTRACT_OR_NONE(ID, "</ID>");
  Ret.ID = std::stoull(std::string{ID});

  CONSUME_OR_NONE("<NONCE>");
  EXTRACT_OR_NONE(Nonce, "</NONCE>");
  Ret.Nonce = std::stoull(std::string{Nonce});

  BASE_FOOTER_OR_NONE("</CLIENT>");
  return Ret;
}

ENCODE_BASE(ProcessSpawnOptions)
{
  std::ostringstream Buf;
  Buf << "<PROCESS>";
  {
    Buf << "<IMAGE>" << Object.Program << "</IMAGE>";
    Buf << "<ARGUMENTS Count=\"" << Object.Arguments.size() << "\">";
    {
      for (const std::string& Arg : Object.Arguments)
        Buf << "<ARGUMENT Size=\"" << Arg.size() << "\">" << Arg
            << "</ARGUMENT>";
    }
    Buf << "</ARGUMENTS>";
    Buf << "<ENVIRONMENT>";
    {
      Buf << "<DEFINE Count=\"" << Object.SetEnvironment.size() << "\">";
      {
        for (const std::pair<std::string, std::string>& EnvKV :
             Object.SetEnvironment)
        {
          Buf << "<VARVAL>";
          {
            Buf << "<VAR Size=\"" << EnvKV.first.size() << "\">" << EnvKV.first
                << "</VAR>";
            Buf << "<VAL Size=\"" << EnvKV.second.size() << "\">"
                << EnvKV.second << "</VAL>";
          }
          Buf << "</VARVAL>";
        }
      }
      Buf << "</DEFINE>";
      Buf << "<UNSET Count=\"" << Object.UnsetEnvironment.size() << "\">";
      {
        for (const std::string& EnvK : Object.UnsetEnvironment)
          Buf << "<VAR Size=\"" << EnvK.size() << "\">" << EnvK << "</VAR>";
      }
      Buf << "</UNSET>";
    }
    Buf << "</ENVIRONMENT>";
  }
  Buf << "</PROCESS>";
  return Buf.str();
}
DECODE_BASE(ProcessSpawnOptions)
{
  ProcessSpawnOptions Ret;
  HEADER_OR_NONE("<PROCESS>");

  CONSUME_OR_NONE("<IMAGE>");
  EXTRACT_OR_NONE(Image, "</IMAGE>");
  Ret.Program = Image;

  {
    CONSUME_OR_NONE("<ARGUMENTS Count=\"");
    EXTRACT_OR_NONE(ArgumentCount, "\">");
    std::size_t ArgC = std::stoull(std::string{ArgumentCount});
    Ret.Arguments.resize(ArgC);
    for (std::size_t I = 0; I < ArgC; ++I)
    {
      CONSUME_OR_NONE("<ARGUMENT Size=\"");
      EXTRACT_OR_NONE(ArgumentSize, "\">");
      if (std::size_t S = std::stoull(std::string{ArgumentSize}))
        Ret.Arguments.at(I) = splice(View, S);

      CONSUME_OR_NONE("</ARGUMENT>");
    }
    CONSUME_OR_NONE("</ARGUMENTS>");
  }

  {
    CONSUME_OR_NONE("<ENVIRONMENT>");
    {
      CONSUME_OR_NONE("<DEFINE Count=\"");
      EXTRACT_OR_NONE(EnvDefineCount, "\">");
      std::size_t SetC = std::stoull(std::string{EnvDefineCount});
      Ret.SetEnvironment.resize(SetC);
      for (std::size_t I = 0; I < SetC; ++I)
      {
        CONSUME_OR_NONE("<VARVAL>");

        CONSUME_OR_NONE("<VAR Size=\"");
        EXTRACT_OR_NONE(VarSize, "\">");
        if (std::size_t S = std::stoull(std::string{VarSize}))
          Ret.SetEnvironment.at(I).first = splice(View, S);
        CONSUME_OR_NONE("</VAR>");

        CONSUME_OR_NONE("<VAL Size=\"");
        EXTRACT_OR_NONE(ValSize, "\">");
        if (std::size_t S = std::stoull(std::string{ValSize}))
          Ret.SetEnvironment.at(I).second = splice(View, S);
        CONSUME_OR_NONE("</VAL>");

        CONSUME_OR_NONE("</VARVAL>");
      }
      CONSUME_OR_NONE("</DEFINE>");
    }
    {
      CONSUME_OR_NONE("<UNSET Count=\"");
      EXTRACT_OR_NONE(EnvUnsetCount, "\">");
      std::size_t UnsetC = std::stoull(std::string{EnvUnsetCount});
      Ret.UnsetEnvironment.resize(UnsetC);
      for (std::size_t I = 0; I < UnsetC; ++I)
      {
        CONSUME_OR_NONE("<VAR Size=\"");
        EXTRACT_OR_NONE(VarSize, "\">");
        if (std::size_t S = std::stoull(std::string{VarSize}))
          Ret.UnsetEnvironment.at(I) = splice(View, S);
        CONSUME_OR_NONE("</VAR>");
      }
      CONSUME_OR_NONE("</UNSET>");
    }
    CONSUME_OR_NONE("</ENVIRONMENT>");
  }

  BASE_FOOTER_OR_NONE("</PROCESS>");
  return Ret;
}

ENCODE_BASE(SessionData)
{
  std::ostringstream Buf;
  Buf << "<SESSION>";
  Buf << "<NAME>" << Object.Name << "</NAME>";
  Buf << "<CREATED>" << Object.Created << "</CREATED>";
  Buf << "</SESSION>";
  return Buf.str();
}
DECODE_BASE(SessionData)
{
  SessionData Ret;
  HEADER_OR_NONE("<SESSION>");

  CONSUME_OR_NONE("<NAME>");
  EXTRACT_OR_NONE(Name, "</NAME>");
  Ret.Name = Name;

  CONSUME_OR_NONE("<CREATED>");
  EXTRACT_OR_NONE(Created, "</CREATED>");
  static_assert(std::is_arithmetic_v<decltype(Ret.Created)>,
                "SessionData::Created should be an arithmetic type.");
  if constexpr (std::is_integral_v<decltype(Ret.Created)>)
  {
    static_assert(std::is_unsigned_v<decltype(Ret.Created)> ||
                    std::is_signed_v<decltype(Ret.Created)>,
                  "Integer should be signed or unsigned!");
    if constexpr (std::is_unsigned_v<decltype(Ret.Created)>)
    {
      Ret.Created = std::stoull(std::string{Created});
    }
    else if constexpr (std::is_signed_v<decltype(Ret.Created)>)
    {
      Ret.Created = std::stoll(std::string{Created});
    }
  }
  else if constexpr (std::is_floating_point_v<decltype(Ret.Created)>)
  {
    Ret.Created = std::stold(std::string{Created});
  }

  BASE_FOOTER_OR_NONE("</SESSION>");
  return Ret;
}

#undef BASE_FOOTER_OR_NONE
#define FOOTER_OR_NONE(LITERAL)                                                \
  if (View != (LITERAL))                                                       \
    return std::nullopt;                                                       \
  View = consume(View, LITERAL);

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
  std::ostringstream Buf;
  Buf << "<DATASOCKET>";
  Buf << monomux::message::ClientID::encode(Object.Client);
  Buf << "</DATASOCKET>";

  return Buf.str();
}
DECODE(DataSocket)
{
  DataSocket Ret;
  HEADER_OR_NONE("<DATASOCKET>");

  auto ClientID = monomux::message::ClientID::decode(View);
  if (!ClientID)
    return std::nullopt;
  Ret.Client = std::move(*ClientID);

  FOOTER_OR_NONE("</DATASOCKET>");
  return Ret;
}

ENCODE(SessionList)
{
  (void)Object;
  return "<SESSION-LIST />";
}
DECODE(SessionList)
{
  if (Buffer == "<SESSION-LIST />")
    return SessionList{};
  return std::nullopt;
}

ENCODE(MakeSession)
{
  std::ostringstream Buf;
  Buf << "<MAKE-SESSION>";
  if (Object.Name.empty())
    Buf << "<NAME />";
  else
    Buf << "<NAME>" << Object.Name << "</NAME>";
  Buf << monomux::message::ProcessSpawnOptions::encode(Object.SpawnOpts);
  Buf << "</MAKE-SESSION>";
  return Buf.str();
}
DECODE(MakeSession)
{
  MakeSession Ret;
  HEADER_OR_NONE("<MAKE-SESSION>");

  PEEK_AND_CONSUME("<NAME />") { Ret.Name = ""; }
  PEEK_AND_CONSUME("<NAME>")
  {
    EXTRACT_OR_NONE(Name, "</NAME>");
    Ret.Name = Name;
  }

  auto Spawn = monomux::message::ProcessSpawnOptions::decode(View);
  if (!Spawn)
    return std::nullopt;
  Ret.SpawnOpts = std::move(*Spawn);

  FOOTER_OR_NONE("</MAKE-SESSION>");
  return Ret;
}

} // namespace request

namespace response
{

ENCODE(ClientID)
{
  std::ostringstream Buf;
  Buf << "<CLIENT-ID>";
  Buf << monomux::message::ClientID::encode(Object.Client);
  Buf << "</CLIENT-ID>";
  return Buf.str();
}
DECODE(ClientID)
{
  ClientID Ret;
  HEADER_OR_NONE("<CLIENT-ID>");

  auto ClientID = monomux::message::ClientID::decode(View);
  if (!ClientID)
    return std::nullopt;
  Ret.Client = std::move(*ClientID);

  FOOTER_OR_NONE("</CLIENT-ID>");
  return Ret;
}

ENCODE(DataSocket)
{
  std::ostringstream Ret;
  Ret << "<DATASOCKET>";
  Ret << (Object.Success ? "<ACCEPT />" : "<DENY />");
  Ret << "</DATASOCKET>";

  using namespace std::string_literals;
  Ret << "!MAINTAIN-RADIO-SILENCE!\0\0\0"s;

  return Ret.str();
}
DECODE(DataSocket)
{
  DataSocket Ret;
  HEADER_OR_NONE("<DATASOCKET>");

  EXTRACT_OR_NONE(Contents, "</DATASOCKET>");
  if (Contents == "<ACCEPT />")
    Ret.Success = true;
  else if (Contents == "<DENY />")
    Ret.Success = false;
  else
    return std::nullopt;

  // Do not use FOOTER_OR_NONE! We ignore the "radio silence" message. :)
  return Ret;
}

ENCODE(SessionList)
{
  std::ostringstream Buf;
  Buf << "<SESSION-LIST Count=\"" << Object.Sessions.size() << "\">";
  for (const SessionData& SD : Object.Sessions)
    Buf << monomux::message::SessionData::encode(SD);
  Buf << "</SESSION-LIST>";
  return Buf.str();
}
DECODE(SessionList)
{
  SessionList Ret;
  HEADER_OR_NONE("<SESSION-LIST Count=\"");

  {
    EXTRACT_OR_NONE(ListCount, "\">");
    std::size_t ListC = std::stoull(std::string{ListCount});
    Ret.Sessions.reserve(ListC);
    for (std::size_t I = 0; I < ListC; ++I)
    {
      auto SD = monomux::message::SessionData::decode(View);
      if (!SD)
        return std::nullopt;
      Ret.Sessions.emplace_back(*std::move(SD));
    }
  }

  FOOTER_OR_NONE("</SESSION-LIST>");
  return Ret;
}

// ENCODE(MakeSession)
// {
//
// }
// DECODE(MakeSession)
// {
//
// }

} // namespace response

} // namespace message
} // namespace monomux

#undef CONSUME_OR_NONE
#undef PEEK_AND_CONSUME
#undef HEADER_OR_NONE
#undef FOOTER_OR_NONE

#undef ENCODE
#undef DECODE_BASE
#undef DECODE
#undef DECODE_BASE
