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

#include <chrono>
#include <ctime>

#include "monomux/control/Message.hpp"

/// Helper function for removing the explicit terminator from the created buffer
/// for ease of testing.
template <typename Msg> static std::string encode(const Msg& M)
{
  using namespace monomux::message;

  std::string S = monomux::message::encode(M);
  std::string_view Unpacked = Message::unpack(S).RawData;
  return std::string{Unpacked};
}

template <typename Msg> static Msg codec(const Msg& M)
{
  using namespace monomux::message;

  std::string Data = monomux::message::encode(M);
  std::optional<Msg> Decode = decode<Msg>(Data);
  EXPECT_TRUE(Decode && "Decoding just encoded message should succeed!");
  return *Decode;
}

TEST(ControlMessageSerialisation, ConnectionNotification)
{
  monomux::message::notification::Connection Obj;
  Obj.Accepted = true;
  EXPECT_EQ(encode(Obj), "<CONNECTION><TRUE /></CONNECTION>");
  EXPECT_EQ(codec(Obj).Accepted, true);

  Obj.Accepted = false;
  EXPECT_EQ(encode(Obj),
            "<CONNECTION><FALSE /><REASON> </REASON></CONNECTION>");
  EXPECT_EQ(codec(Obj).Accepted, false);
  EXPECT_EQ(codec(Obj).Reason, "");

  Obj.Reason = "Bad intent";
  EXPECT_EQ(encode(Obj),
            "<CONNECTION><FALSE /><REASON>Bad intent </REASON></CONNECTION>");
  EXPECT_EQ(codec(Obj).Accepted, false);
  EXPECT_EQ(codec(Obj).Reason, "Bad intent");

  Obj.Accepted = true;
  EXPECT_EQ(encode(Obj), "<CONNECTION><TRUE /></CONNECTION>");
  EXPECT_EQ(codec(Obj).Accepted, true);
}

TEST(ControlMessageSerialisation, ClientIDRequest)
{
  EXPECT_EQ(encode(monomux::message::request::ClientID{}), "<CLIENT-ID />");
}

TEST(ControlMessageSerialisation, ClientIDResponse)
{
  monomux::message::response::ClientID Obj;
  Obj.Client.ID = 4;
  Obj.Client.Nonce = 2;
  EXPECT_EQ(
    encode(Obj),
    "<CLIENT-ID><CLIENT><ID>4</ID><NONCE>2</NONCE></CLIENT></CLIENT-ID>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, DataSocketRequest)
{
  monomux::message::request::DataSocket Obj;
  Obj.Client.ID = 2;
  Obj.Client.Nonce = 3;
  EXPECT_EQ(
    encode(Obj),
    "<DATASOCKET><CLIENT><ID>2</ID><NONCE>3</NONCE></CLIENT></DATASOCKET>");

  auto Decode = codec(Obj);
  EXPECT_EQ(Obj.Client.ID, Decode.Client.ID);
  EXPECT_EQ(Obj.Client.Nonce, Decode.Client.Nonce);
}

TEST(ControlMessageSerialisation, DataSocketRespons)
{
  {
    monomux::message::response::DataSocket Obj;
    Obj.Success = true;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET><TRUE /></DATASOCKET>") == 0);

    auto Decode = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode.Success);
  }

  {
    monomux::message::response::DataSocket Obj;
    Obj.Success = false;
    EXPECT_TRUE(encode(Obj).find("<DATASOCKET><FALSE /></DATASOCKET>") == 0);

    auto Decode2 = codec(Obj);
    EXPECT_EQ(Obj.Success, Decode2.Success);
  }
}

TEST(ControlMessageSerialisation, SessionListRequest)
{
  EXPECT_EQ(encode(monomux::message::request::SessionList{}),
            "<SESSION-LIST />");
}

TEST(ControlMessageSerialisation, SessionListResponse)
{
  monomux::message::response::SessionList Obj;
  std::time_t CurrentTimeEncoded =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

  Obj.Sessions.push_back({});
  Obj.Sessions.at(0).Name = "Foo";
  Obj.Sessions.at(0).Created = CurrentTimeEncoded;

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Sessions.size(), 1);
    EXPECT_EQ(Decode.Sessions.at(0).Name, "Foo");
    EXPECT_EQ(Decode.Sessions.at(0).Created, CurrentTimeEncoded);
  }

  std::time_t CurrentTimeEncoded2 =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  Obj.Sessions.push_back({});
  Obj.Sessions.at(1).Name = "Bar";
  Obj.Sessions.at(1).Created = CurrentTimeEncoded2;

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Sessions.size(), 2);
    EXPECT_EQ(Decode.Sessions.at(0).Name, "Foo");
    EXPECT_EQ(Decode.Sessions.at(0).Created, CurrentTimeEncoded);
    EXPECT_EQ(Decode.Sessions.at(1).Name, "Bar");
    EXPECT_EQ(Decode.Sessions.at(1).Created, CurrentTimeEncoded2);
  }
}

TEST(ControlMessageSerialisation, MakeSessionRequest)
{
  monomux::message::request::MakeSession Obj;
  Obj.SpawnOpts.Program = "/bin/bash";

  {
    auto Decode = codec(Obj);
    EXPECT_TRUE(Decode.Name.empty());
    EXPECT_EQ(Decode.SpawnOpts.Program, "/bin/bash");
    EXPECT_TRUE(Decode.SpawnOpts.Arguments.empty());
    EXPECT_TRUE(Decode.SpawnOpts.SetEnvironment.empty());
    EXPECT_TRUE(Decode.SpawnOpts.UnsetEnvironment.empty());
  }

  Obj.Name = "Foo";

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

TEST(ControlMessageSerialisation, MakeSessionResponse)
{
  monomux::message::response::MakeSession Obj;
  Obj.Name = "Foo";
  Obj.Success = false;

  EXPECT_EQ(encode(Obj),
            "<MAKE-SESSION><FALSE /><NAME>Foo</NAME></MAKE-SESSION>");

  {
    auto Decode = codec(Obj);
    EXPECT_FALSE(Decode.Success);
    EXPECT_EQ(Decode.Name, "Foo");
  }

  Obj.Name = "Bar";
  Obj.Success = true;

  {
    auto Decode = codec(Obj);
    EXPECT_TRUE(Decode.Success);
    EXPECT_EQ(Decode.Name, "Bar");
  }
}

TEST(ControlMessageSerialisation, AttachRequest)
{
  monomux::message::request::Attach Obj;
  Obj.Name = "Foo";

  EXPECT_EQ(encode(Obj), "<ATTACH><NAME>Foo</NAME></ATTACH>");

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Name, "Foo");
  }

  Obj.Name = "Bar";

  EXPECT_EQ(encode(Obj), "<ATTACH><NAME>Bar</NAME></ATTACH>");

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(Decode.Name, "Bar");
  }
}

TEST(ControlMessageSerialisation, AttachResponse)
{
  monomux::message::response::Attach Obj;
  Obj.Success = false;

  EXPECT_EQ(encode(Obj), "<ATTACH><FALSE /></ATTACH>");

  {
    auto Decode = codec(Obj);
    EXPECT_FALSE(Decode.Success);
  }


  std::time_t CurrentTimeEncoded =
    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  Obj.Success = true;
  Obj.Session.Name = "Foo";
  Obj.Session.Created = CurrentTimeEncoded;

  {
    auto Decode = codec(Obj);
    EXPECT_TRUE(Decode.Success);
    EXPECT_EQ(Decode.Session.Name, Obj.Session.Name);
    EXPECT_EQ(Decode.Session.Created, Obj.Session.Created);
  }
}

TEST(ControlMessageSerialisation, DetachRequest)
{
  using namespace monomux::message::request;

  Detach Obj;
  Obj.Mode = Detach::Latest;
  EXPECT_EQ(encode(Obj), "<DETACH><MODE>Latest</MODE></DETACH>");
  EXPECT_EQ(codec(Obj).Mode, Detach::Latest);

  Obj.Mode = Detach::All;
  EXPECT_EQ(encode(Obj), "<DETACH><MODE>All</MODE></DETACH>");
  EXPECT_EQ(codec(Obj).Mode, Detach::All);
}

TEST(ControlMessageSerialisation, DetachResponse)
{
  EXPECT_EQ(encode(monomux::message::response::Detach{}), "<DETACH />");
}

TEST(ControlMessageSerialisation, DetachedNotification)
{
  using namespace monomux::message::notification;
  Detached Obj;
  Obj.ExitCode = 2;
  Obj.Reason = "Test";

  Obj.Mode = Detached::Detach;
  EXPECT_EQ(encode(Obj), "<DETACHED><MODE>Detach</MODE></DETACHED>");
  EXPECT_EQ(codec(Obj).Mode, Detached::Detach);
  EXPECT_EQ(codec(Obj).ExitCode, 0);
  EXPECT_TRUE(codec(Obj).Reason.empty());

  Obj.Mode = Detached::Exit;
  EXPECT_EQ(encode(Obj),
            "<DETACHED><MODE>Exit</MODE><CODE>2</CODE></DETACHED>");
  EXPECT_EQ(codec(Obj).Mode, Detached::Exit);
  EXPECT_EQ(codec(Obj).ExitCode, 2);
  EXPECT_TRUE(codec(Obj).Reason.empty());

  Obj.Mode = Detached::ServerShutdown;
  EXPECT_EQ(encode(Obj), "<DETACHED><MODE>Server</MODE></DETACHED>");
  EXPECT_EQ(codec(Obj).Mode, Detached::ServerShutdown);
  EXPECT_EQ(codec(Obj).ExitCode, 0);
  EXPECT_TRUE(codec(Obj).Reason.empty());

  Obj.Mode = Detached::Kicked;
  EXPECT_EQ(encode(Obj),
            "<DETACHED><MODE>Booted</MODE><REASON>Test</REASON></DETACHED>");
  EXPECT_EQ(codec(Obj).Mode, Detached::Kicked);
  EXPECT_EQ(codec(Obj).ExitCode, 0);
  EXPECT_EQ(codec(Obj).Reason, Obj.Reason);
}

TEST(ControlMessageSerialisation, SignalRequest)
{
  monomux::message::request::Signal Obj;
  Obj.SigNum = 1;
  EXPECT_EQ(encode(Obj), "<SIGNAL>1</SIGNAL>");
  EXPECT_EQ(codec(Obj).SigNum, 1);
}

TEST(ControlMessageSerialisation, RedrawNotification)
{
  monomux::message::notification::Redraw Obj;
  Obj.Columns = 80; // NOLINT(readability-magic-numbers)
  Obj.Rows = 24;    // NOLINT(readability-magic-numbers)

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(encode(Obj),
              "<WINDOW-SIZE-CHANGE><ROWS>24</ROWS><COLS>80</COLS>"
              "</WINDOW-SIZE-CHANGE>");
    EXPECT_EQ(Decode.Rows, Obj.Rows);
    EXPECT_EQ(Decode.Columns, Obj.Columns);
  }
}

TEST(ControlMessageSerialisation, StatisticsRequest)
{
  monomux::message::request::Statistics Obj;
  EXPECT_EQ(encode(Obj), "<SEND-STATISTICS />");
}

TEST(ControlMessageSerialisation, StatisticsResponse)
{
  monomux::message::response::Statistics Obj;
  Obj.Contents = "Foo";

  {
    auto Decode = codec(Obj);
    EXPECT_EQ(encode(Obj), "<STATISTICS Size=\"3\">Foo</STATISTICS>");
    EXPECT_EQ(Decode.Contents, Obj.Contents);
  }
}
