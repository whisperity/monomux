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
#include <gtest/gtest.h>

#include "control/Message.hpp"

/// Helper function for removing the explicit terminator from the created buffer
/// for ease of testing.
template <typename Msg> static std::string encode(const Msg& M)
{
  using namespace monomux;

  std::string S = monomux::encode(M);
  std::string_view Unpacked = MessageBase::unpack(S).RawData;
  return std::string{Unpacked};
}

template <typename Msg> static Msg codec(const Msg& M)
{
  using namespace monomux;

  std::string Data = monomux::encode(M);
  std::optional<Msg> Decode = decode<Msg>(Data);
  assert(Decode);
  return *Decode;
}

TEST(ControlMessageSerialisation, REQ_ClientID)
{
  EXPECT_EQ(encode(monomux::request::ClientID{}), "<CLIENT-ID />");
}

TEST(ControlMessageSerialisation, RSP_ClientID)
{
  monomux::response::ClientID Obj;
  Obj.Client.ID = 42;
  Obj.Client.Nonce = 16;
  EXPECT_EQ(encode(Obj), "<CLIENT-ID>42<NONCE>16</NONCE></CLIENT-ID>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, REQ_DataSocket)
{
  monomux::request::DataSocket Obj;
  Obj.Client.ID = 55;
  Obj.Client.Nonce = 177;
  EXPECT_EQ(
    encode(Obj),
    "<DATASOCKET><CLIENT-ID>55<NONCE>177</NONCE></CLIENT-ID></DATASOCKET>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, RSP_DataSocket)
{
  {
    monomux::response::DataSocket Obj;
    Obj.Success = true;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET>Accept</DATASOCKET>") == 0);

    auto Decode = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode.Success);
  }

  {
    monomux::response::DataSocket Obj;
    Obj.Success = false;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET>Deny</DATASOCKET>") == 0);

    auto Decode2 = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode2.Success);
  }
}
