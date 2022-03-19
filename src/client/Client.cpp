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

std::optional<Client> Client::create(std::string SocketPath,
                                     std::string* RejectReason)
{
  using namespace monomux::message;

  try
  {
    Socket S = Socket::connect(std::move(SocketPath));
    std::optional<notification::Connection> ConnStatus =
      receiveMessage<notification::Connection>(S);
    if (!ConnStatus)
      return std::nullopt;
    if (!ConnStatus->Accepted)
    {
      if (RejectReason)
        *RejectReason = std::move(ConnStatus->Reason);
      return std::nullopt;
    }

    Client Conn{std::move(S)};
    return Conn;
  }
  catch (const std::system_error& Err)
  {
    throw;
  }
  return std::nullopt;
}

Client::Client(Socket&& ControlSock) : ControlSocket(std::move(ControlSock))
{
  setUpDispatch();
}

void Client::registerMessageHandler(std::uint16_t Kind,
                                    std::function<HandlerFunction> Handler)
{
  Dispatch[Kind] = std::move(Handler);
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

bool Client::handshake(std::string* FailureReason)
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
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "establish connection.";
      return false;
    }
    responseClientID(MB.RawData);
    if (ClientID == static_cast<std::size_t>(-1) && !Nonce.has_value())
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "establish connection.";
      return false;
    }
  }

  // If the control socket is now successfully established, establish another
  // connection to the same location, but for the data socket.
  auto DS =
    std::make_unique<Socket>(Socket::connect(ControlSocket.identifier()));
  {
    // See if the server successfully accepted the second connection.
    std::optional<notification::Connection> ConnStatus =
      receiveMessage<notification::Connection>(*DS);
    if (!ConnStatus)
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "establish Data connection.";
      return false;
    }
    if (!ConnStatus->Accepted)
    {
      if (FailureReason)
        *FailureReason = std::move(ConnStatus->Reason);
      return false;
    }
  }
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
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "establish Data connection.";
      return false;
    }
    if (!Response->Success)
    {
      if (FailureReason)
        *FailureReason =
          "ERROR: Server rejected establishment of Data connection...";
      return false;
    }
  }
  DataSocket = std::move(DS);

  // After a successful data connection establishment, the Nonce value was
  // consumed, so we need to request a new one.
  {
    sendMessage(ControlSocket, request::ClientID{});

    // We decode the response message to be able to fire the handler manually.
    std::string Data = readPascalString(ControlSocket);
    Message MB = Message::unpack(Data);
    if (MB.Kind != MessageKind::ClientIDResponse)
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "sign off connection.";
      return false;
    }
    responseClientID(MB.RawData);
    if (ClientID == static_cast<std::size_t>(-1) && !Nonce.has_value())
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "sign off connection.";
      return false;
    }
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

  enableControlResponsePoll();
  enableDataSocket();

  while (!TerminateLoop.get().load())
  {
    const std::size_t NumTriggeredFDs = Poll->wait();
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      raw_fd EventFD = Poll->fdAt(I);
      if (EventFD == DataSocket->raw())
      {
        dataCallback();
        continue;
      }
      if (EventFD == Term->input())
      {
        inputCallback();
        continue;
      }
      if (EventFD == ControlSocket.raw())
      {
        controlCallback();
        continue;
      }
    }
  }

  disableDataSocket();
  inhibitControlResponsePoll();
}

void Client::dataCallback()
{
  std::string Data = DataSocket->read(256);
  ::write(Term->output(), Data.c_str(), Data.size());
}

void Client::inputCallback()
{
  POD<char[512]> Buf;
  unsigned long Size = ::read(Term->input(), &Buf, sizeof(Buf));
  if (!DataSocketEnabled)
    return;
  sendData(std::string_view{&Buf[0], Size});
}

void Client::controlCallback()
{
  using namespace monomux::message;
  std::string Data;
  try
  {
    Data = readPascalString(ControlSocket);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "ERROR: When reading from control:" << Err.what() << std::endl;
  }

  if (ControlSocket.failed())
  {
    exit(Failed);
    return;
  }

  if (Data.empty())
    return;

  std::cout << Data << std::endl;

  std::cout << "Check for message kind... ";
  Message MB = Message::unpack(Data);
  auto Action =
    Dispatch.find(static_cast<decltype(Dispatch)::key_type>(MB.Kind));
  if (Action == Dispatch.end())
  {
    std::cerr << "Error: Unknown message type " << static_cast<int>(MB.Kind)
              << " received." << std::endl;
    return;
  }
  std::cout << static_cast<int>(MB.Kind) << std::endl;

  std::cout << "Read data " << std::string_view{Data.data(), Data.size()}
            << std::endl;
  try
  {
    Action->second(MB.RawData);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when handling message " << std::endl;
    if (getControlSocket().failed())
      exit(Failed);
    return;
  }
}

void Client::exit(ExitReason E)
{
  std::clog << "TRACE: Client exit " << E << std::endl;
  Exit = E;
  Poll.reset();
  TerminateLoop.get().store(true);
}

void Client::setupPoll()
{
  static constexpr std::size_t EventQueue = 1 << 9;
  if (!Poll)
    Poll = std::make_unique<EPoll>(EventQueue);
  else
    Poll->clear();
}

void Client::enableDataSocket()
{
  if (!Poll || !DataSocket)
    return;
  Poll->listen(DataSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
  DataSocketEnabled = true;
}

void Client::disableDataSocket()
{
  if (!Poll || !DataSocket)
    return;
  Poll->stop(DataSocket->raw());
  DataSocketEnabled = false;
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

std::optional<std::string>
Client::requestMakeSession(std::string Name, Process::SpawnOptions Opts)
{
  using namespace monomux::message;
  auto X = scopedInhibitControlResponsePoll();

  request::MakeSession Msg;
  Msg.Name = std::move(Name);
  Msg.SpawnOpts.Program = std::move(Opts.Program);
  Msg.SpawnOpts.Arguments = std::move(Opts.Arguments);
  for (std::pair<const std::string, std::optional<std::string>>& E :
       Opts.Environment)
  {
    if (!E.second)
      Msg.SpawnOpts.UnsetEnvironment.emplace_back(E.first);
    else
      Msg.SpawnOpts.SetEnvironment.emplace_back(E.first, std::move(*E.second));
  }
  sendMessage(ControlSocket, Msg);

  std::optional<response::MakeSession> Resp =
    receiveMessage<response::MakeSession>(ControlSocket);
  if (!Resp || !Resp->Success)
    return std::nullopt;

  return std::move(Resp->Name);
}

bool Client::requestAttach(std::string SessionName)
{
  using namespace monomux::message;
  auto X = scopedInhibitControlResponsePoll();

  request::Attach Msg;
  Msg.Name = std::move(SessionName);
  sendMessage(ControlSocket, Msg);

  std::optional<response::Attach> Resp =
    receiveMessage<response::Attach>(ControlSocket);
  if (!Resp)
    Attached = false;
  else
    Attached = Resp->Success;

  return Attached;
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
