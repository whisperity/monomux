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
  EXPECT_TRUE(Decode && "Decoding just encoded message should succeed!");
  return *Decode;
}

TEST(ControlMessageSerialisation, ClientIDRequest)
{
  EXPECT_EQ(encode(monomux::request::ClientID{}), "<CLIENT-ID />");
}

TEST(ControlMessageSerialisation, ClientIDResponse)
{
  monomux::response::ClientID Obj;
  Obj.Client.ID = 42;
  Obj.Client.Nonce = 16;
  EXPECT_EQ(
    encode(Obj),
    "<CLIENT-ID><CLIENT><ID>42</ID><NONCE>16</NONCE></CLIENT></CLIENT-ID>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, DataSocketRequest)
{
  monomux::request::DataSocket Obj;
  Obj.Client.ID = 55;
  Obj.Client.Nonce = 177;
  EXPECT_EQ(
    encode(Obj),
    "<DATASOCKET><CLIENT><ID>55</ID><NONCE>177</NONCE></CLIENT></DATASOCKET>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, DataSocketRespons)
{
  {
    monomux::response::DataSocket Obj;
    Obj.Success = true;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET><ACCEPT /></DATASOCKET>") == 0);

    auto Decode = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode.Success);
  }

  {
    monomux::response::DataSocket Obj;
    Obj.Success = false;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET><DENY /></DATASOCKET>") == 0);

    auto Decode2 = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode2.Success);
  }
}

TEST(ControlMessageSerialisation, MakeSessionRequest)
{
  monomux::request::MakeSession Obj;
  Obj.Name = "Foo";
  Obj.SpawnOpts.Program = "/bin/bash";

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Name, "Foo");
    EXPECT_EQ(Decode.SpawnOpts.Program, "/bin/bash");
    EXPECT_TRUE(Decode.SpawnOpts.Arguments.empty());
    EXPECT_TRUE(Decode.SpawnOpts.SetEnvironment.empty());
    EXPECT_TRUE(Decode.SpawnOpts.UnsetEnvironment.empty());
  }

  Obj.SpawnOpts.Arguments.emplace_back("--norc");
  Obj.SpawnOpts.Arguments.emplace_back("--interactive");
  Obj.SpawnOpts.SetEnvironment.emplace_back("SHLVL", "8");
  Obj.SpawnOpts.UnsetEnvironment.emplace_back("TERM");

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Name, "Foo");
    EXPECT_EQ(Decode.SpawnOpts.Program, "/bin/bash");
    EXPECT_EQ(Decode.SpawnOpts.Arguments.size(), 2);
    EXPECT_EQ(Decode.SpawnOpts.Arguments.at(0), "--norc");
    EXPECT_EQ(Decode.SpawnOpts.Arguments.at(1), "--interactive");
    EXPECT_EQ(Decode.SpawnOpts.SetEnvironment.size(), 1);
    EXPECT_EQ(Decode.SpawnOpts.SetEnvironment.at(0).first, "SHLVL");
    EXPECT_EQ(Decode.SpawnOpts.SetEnvironment.at(0).second, "8");
    EXPECT_EQ(Decode.SpawnOpts.UnsetEnvironment.size(), 1);
    EXPECT_EQ(Decode.SpawnOpts.UnsetEnvironment.at(0), "TERM");
  }
}
