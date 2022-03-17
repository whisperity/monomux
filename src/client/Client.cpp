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
#include "Client.hpp"

#include "control/Message.hpp"
#include "control/Messaging.hpp"
#include "system/Pipe.hpp"

#include <iostream>
#include <utility>

namespace monomux
{
namespace client
{

std::optional<Client> Client::create(std::string SocketPath)
{
  try
  {
    Client Conn{Socket::connect(SocketPath)};
    return Conn;
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "When creating ServerConnection with '" << SocketPath
              << "': " << Err.what() << std::endl;
  }
  return std::nullopt;
}

Client::Client(Socket&& ControlSock) : ControlSocket(std::move(ControlSock))
{
  setUpDispatch();
}

void Client::setTerminal(Terminal&& T)
{
  if (Term)
    Poll->stop(Term->input());

  Term.emplace(std::move(T));
  Poll->listen(Term->input(), /* Incoming =*/true, /* Outgoing =*/false);
}

void Client::setDataSocket(Socket&& DataSocket)
{
  if (Poll)
    Poll->stop(this->DataSocket->raw());

  this->DataSocket = std::make_unique<Socket>(std::move(DataSocket));

  if (Poll)
    Poll->listen(
      this->DataSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
}

bool Client::handshake()
{
  using namespace monomux::message;

  // Authenticate the client on the server.
  {
    sendMessage(ControlSocket, request::ClientID{});

    // We decode the response message to be able to fire the handler manually.
    std::string Data = readPascalString(ControlSocket);
    Message MB = Message::unpack(Data);
    if (MB.Kind != MessageKind::ClientIDResponse)
    {
      std::cerr
        << "ERROR: Invalid response from Server when trying to establish "
           "connection."
        << std::endl;
      return false;
    }
    responseClientID(MB.RawData);
    if (ClientID == static_cast<std::size_t>(-1) && !Nonce.has_value())
    {
      std::cerr
        << "ERROR: Invalid response from Server when trying to establish "
           "connection."
        << std::endl;
      return false;
    }
  }

  // If the control socket is now successfully established, establish another
  // connection to the same location, but for the data socket.
  auto DS =
    std::make_unique<Socket>(Socket::connect(ControlSocket.identifier()));
  // (At this point, the server believes a brand new client has connected, and
  // is awaiting that client's handshake request. Instead, use our identity to
  // tell the server that this connection is in fact the same client, but it's
  // data connection.)
  {
    request::DataSocket Req;
    Req.Client.ID = ClientID;
    Req.Client.Nonce = consumeNonce();
    sendMessage(*DS, Req);

    std::optional<response::DataSocket> Response =
      receiveMessage<response::DataSocket>(*DS);
    if (!Response.has_value())
    {
      std::cerr << "ERROR: Invalid response from Server when trying to "
                   "establish Data connection."
                << std::endl;
      return false;
    }
    if (!Response->Success)
    {
      std::cerr << "ERROR: Server rejected establishment of Data connection..."
                << std::endl;
      return false;
    }

    DataSocket = std::move(DS);

    // TODO: Request a new nonce here and store it, as the original one got
    // consumed.
  }

  setupPoll();
  return true;
}

void Client::loop()
{
  if (!DataSocket)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client not connected to Server."};
  if (!Term)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client not connected to PTY."};
  if (!Poll)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client event handler not set up."};

  while (TerminateLoop.get().load() == false)
  {
    const std::size_t NumTriggeredFDs = Poll->wait();
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      raw_fd EventFD = Poll->fdAt(I);

      // FIXME: Use Pipes here.

      if (EventFD == DataSocket->raw())
      {
        std::string Data = DataSocket->read(256);
        ::write(Term->output(), Data.c_str(), Data.size());
        continue;
      }
      if (EventFD == Term->input())
      {
        POD<char[512]> Buf;
        unsigned long Size = ::read(Term->input(), &Buf, sizeof(Buf));
        sendData(std::string_view{&Buf[0], Size});
        continue;
      }
      if (EventFD == ControlSocket.raw())
      {
        std::clog << "DEBUG: Handler received on Control connection."
                  << std::endl;
        // FIXME: Not yet implemented.
        continue;
      }
    }
  }
}

void Client::setupPoll()
{
  if (!Poll)
    Poll = std::make_unique<EPoll>(2);
  else
    Poll->clear();

  enableControlResponsePoll();
  if (DataSocket)
    Poll->listen(DataSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
}

void Client::enableControlResponsePoll()
{
  if (!Poll)
    return;
  Poll->listen(ControlSocket.raw(), /* Incoming =*/true, /* Outgoing =*/false);
}

void Client::inhibitControlResponsePoll()
{
  if (!Poll)
    return;
  Poll->stop(ControlSocket.raw());
}

Client::ControlPollInhibitor::ControlPollInhibitor(Client& C) : C(C)
{
  C.inhibitControlResponsePoll();
}

Client::ControlPollInhibitor::~ControlPollInhibitor()
{
  C.enableControlResponsePoll();
}

Client::ControlPollInhibitor Client::scopedInhibitControlResponsePoll()
{
  return ControlPollInhibitor{*this};
}

std::size_t Client::consumeNonce() noexcept
{
  assert(Nonce.has_value());
  auto R = Nonce.value();
  Nonce.reset();
  return R;
}

std::optional<std::vector<SessionData>> Client::requestSessionList()
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;
  using namespace monomux::message;
  auto X = scopedInhibitControlResponsePoll();

  sendMessage(ControlSocket, request::SessionList{});

  std::optional<response::SessionList> Resp =
    receiveMessage<response::SessionList>(ControlSocket);
  if (!Resp)
    return std::nullopt;

  std::vector<SessionData> R;

  for (monomux::message::SessionData& TransmitData : Resp->Sessions)
  {
    SessionData SD;
    SD.Name = std::move(TransmitData.Name);
    SD.Created = std::chrono::system_clock::from_time_t(TransmitData.Created);

    R.emplace_back(std::move(SD));
  }

  return R;
}

bool Client::requestMakeSession(std::string Name, Process::SpawnOptions Opts)
{
  using namespace monomux::message;
  auto X = scopedInhibitControlResponsePoll();

  request::MakeSession Msg;
  Msg.Name = std::move(Name);
  Msg.SpawnOpts.Program = Opts.Program;
  Msg.SpawnOpts.Arguments = Opts.Arguments;
  for (std::pair<const std::string, std::optional<std::string>>& E :
       Opts.Environment)
  {
    if (!E.second)
      Msg.SpawnOpts.UnsetEnvironment.emplace_back(E.first);
    else
      Msg.SpawnOpts.SetEnvironment.emplace_back(E.first, std::move(*E.second));
  }
  sendMessage(ControlSocket, Msg);

  // TODO: Wait for a response!
  return true;
}

void Client::sendData(std::string_view Data)
{
  if (!DataSocket)
  {
    std::cerr
      << "ERROR: Trying to send data but data connection was not established."
      << std::endl;
    return;
  }
  DataSocket->write(Data);
}

} // namespace client
} // namespace monomux
