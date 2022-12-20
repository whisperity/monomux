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
#include <utility>

#include "monomux/Config.h"
#include "monomux/message/Message.hpp"
#include "monomux/message/PascalString.hpp"
#include "monomux/system/BufferedChannel.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/Pipe.hpp"

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/EPoll.hpp"
#include "monomux/system/UnixDomainSocket.hpp"
#include "monomux/system/fd.hpp"
#endif /* MONOMUX_PLATFORM_UNIX */

#include "monomux/client/Client.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("client/Client")

namespace monomux::client
{

std::optional<Client> Client::create(std::string SocketPath,
                                     std::string* RejectReason)
{
  using namespace monomux::message;
  using namespace monomux::system;

  try
  {
    std::unique_ptr<system::Socket> S;

#if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_ID_Unix
    S = std::make_unique<unix::DomainSocket>(
      unix::DomainSocket::connect(std::move(SocketPath)));
#else  /* Unhandled platform */
    (void)SocketPath;
    std::ostringstream OS;
    OS << MONOMUX_FEED_PLATFORM_NOT_SUPPORTED_MESSAGE
       << "socket-based communication, but this is required to connect to "
          "servers.";
    if (RejectReason)
      *RejectReason = OS.str();
    LOG(fatal) << OS.str();
    return std::nullopt;
#endif /* MONOMUX_PLATFORM_ID */

    std::optional<notification::Connection> ConnStatus =
      receiveMessage<notification::Connection>(*S);
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
    if (RejectReason)
      *RejectReason = Err.what();
    throw;
  }
  return std::nullopt;
}

Client::Client(std::unique_ptr<system::Socket>&& ControlSock)
  : ControlSocket(std::move(ControlSock))
{
  setUpDispatch();
}

void Client::registerMessageHandler(std::uint16_t Kind,
                                    std::function<HandlerFunction> Handler)
{
  Dispatch[Kind] = std::move(Handler);
}

void Client::setDataSocket(std::unique_ptr<system::Socket>&& DataSocket)
{
  bool PreviousDataSocketWasEnabled = DataSocketEnabled;
  if (PreviousDataSocketWasEnabled)
    disableDataSocket();

  this->DataSocket = std::move(DataSocket);

  if (PreviousDataSocketWasEnabled)
    enableDataSocket();
}

void Client::setInputFile(system::Handle::Raw FD)
{
  bool PreviousInputFileWasEnabled = InputFileEnabled;
  if (PreviousInputFileWasEnabled)
    disableInputFile();

  InputFile = FD;
  if (!system::Handle::isValid(FD))
    return;

  if (PreviousInputFileWasEnabled)
    enableInputFile();
}

bool Client::handshake(std::string* FailureReason)
{
  using namespace monomux::message;
  using namespace monomux::system;

  // Authenticate the client on the server.
  {
    sendMessage(*ControlSocket, request::ClientID{});

    // We decode the response message to be able to fire the handler manually.
    std::string Data = readPascalString(*ControlSocket);
    Message MB = Message::unpack(Data);
    if (MB.Kind != MessageKind::ClientIDResponse)
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "establish connection.";
      return false;
    }
    responseClientID(*this, MB.RawData);
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
  std::unique_ptr<system::Socket> DS;

#if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_ID_Unix
  DS = std::make_unique<system::unix::DomainSocket>(
    system::unix::DomainSocket::connect(ControlSocket->identifier()));
#else  /* Unhandled platform */
  std::ostringstream OS;
  OS << MONOMUX_FEED_PLATFORM_NOT_SUPPORTED_MESSAGE
     << "socket-based communication, but this is required to connect to "
        "servers.";
  if (FailureReason)
    *FailureReason = OS.str();
  LOG(fatal) << OS.str();
  return false;
#endif /* MONOMUX_PLATFORM_ID */

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
    sendMessage(*ControlSocket, request::ClientID{});

    // We decode the response message to be able to fire the handler manually.
    std::string Data = readPascalString(*ControlSocket);
    Message MB = Message::unpack(Data);
    if (MB.Kind != MessageKind::ClientIDResponse)
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "sign off connection.";
      return false;
    }
    responseClientID(*this, MB.RawData);
    if (ClientID == static_cast<std::size_t>(-1) && !Nonce.has_value())
    {
      if (FailureReason)
        *FailureReason = "ERROR: Invalid response from Server when trying to "
                         "sign off connection.";
      return false;
    }
  }

  return true;
}

void Client::loop()
{
  using namespace monomux::system;

  if (!Handle::isValid(InputFile))
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client input is not connceted."};
  if (!DataSocket)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client is not connected to Server."};
  if (!DataHandler)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client receive callback is not registered."};
  if (!InputHandler)
    throw std::system_error{std::make_error_code(std::errc::not_connected),
                            "Client input callback is not registered."};

  static constexpr std::size_t EventQueue = 1 << 4;

#ifdef MONOMUX_PLATFORM_UNIX
  Poll = std::make_unique<unix::EPoll>(EventQueue);

  unix::fd::addStatusFlag(ControlSocket->raw(), O_NONBLOCK);
  unix::fd::addStatusFlag(DataSocket->raw(), O_NONBLOCK);
#endif /* MONOMUX_PLATFORM_UNIX */

  if (!Poll)
  {
    LOG(fatal) << "No I/O Event poll was created, but this is a critical "
                  "needed functionality.";
    return;
  }

  enableControlResponse();
  enableDataSocket();
  enableInputFile();

  while (!TerminateLoop.get().load())
  {
    ControlSocket->flushWrites();
    if (ExternalEventProcessor)
      // Process "external" events before blocking on "wait()".
      ExternalEventProcessor(*this);
    ControlSocket->tryFreeResources();
    DataSocket->tryFreeResources();

    const std::size_t NumTriggeredFDs = Poll->wait();
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      IOEvent::EventWithMode Event;
      try
      {
        Event = Poll->eventAt(I);
      }
      catch (...)
      {
        Event.FD = PlatformSpecificHandleTraits::Invalid;
      }
      if (!Handle::isValid(Event.FD))
      {
        LOG(error) << '#' << I
                   << " event received but there was no associated file";
        continue;
      }

      try
      {
        if (Event.FD == DataSocket->raw())
        {
          if (Event.Incoming)
          {
            if (DataHandler)
              DataHandler(*this);

            if (DataSocket->hasBufferedRead())
              Poll->schedule(
                DataSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
          }
          if (Event.Outgoing)
          {
            DataSocket->flushWrites();
            if (DataSocket->hasBufferedWrite())
              Poll->schedule(
                DataSocket->raw(), /* Incoming =*/false, /* Outgoing =*/true);
          }
          continue;
        }
        if (Handle::isValid(InputFile) && Event.FD == InputFile)
        {
          if (Event.Incoming && InputHandler)
            InputHandler(*this);
          continue;
        }
        if (Event.FD == ControlSocket->raw() && Event.Incoming)
        {
          controlCallback();
          continue;
        }
      }
      catch (const BufferedChannel::OverflowError& BO)
      {
        Poll->schedule(BO.fd(), BO.readOverflow(), BO.writeOverflow());
      }
      catch (const std::system_error&)
      {
        // Ignore the error on the sockets and pipes, and do not tear the
        // client down just because of them.
      }
    }
  }

  disableInputFile();
  disableDataSocket();
  disableControlResponse();
}

void Client::controlCallback()
{
  using namespace monomux::message;
  std::string Data;

  try
  {
    Data = readPascalString(*ControlSocket);
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    LOG(error) << "Reading CONTROL: "
               << "\n\t" << BO.what();
    Poll->schedule(
      ControlSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
    return;
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Reading CONTROL: " << Err.what();
  }

  if (ControlSocket->failed())
  {
    exit(Failed, -1, "");
    return;
  }

  if (ControlSocket->hasBufferedRead())
    Poll->schedule(
      ControlSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);

  if (Data.empty())
    return;

  Message MB = Message::unpack(Data);
  auto Action =
    Dispatch.find(static_cast<decltype(Dispatch)::key_type>(MB.Kind));
  if (Action == Dispatch.end())
  {
    MONOMUX_TRACE_LOG(LOG(trace) << "Unknown message type "
                                 << static_cast<int>(MB.Kind) << " received");
    return;
  }

  MONOMUX_TRACE_LOG(LOG(data) << MB.RawData);
  try
  {
    Action->second(*this, MB.RawData);
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    LOG(error) << "Error when handling message"
               << "\n\t" << BO.what();
    Poll->schedule(BO.fd(), BO.readOverflow(), BO.writeOverflow());
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Error when handling message";
    if (getControlSocket().failed())
      exit(Failed, -1, "");
  }
}

void Client::setDataCallback(std::function<RawCallbackFn> Callback)
{
  DataHandler = std::move(Callback);
}

void Client::setInputCallback(std::function<RawCallbackFn> Callback)
{
  InputHandler = std::move(Callback);
}

void Client::setExternalEventProcessor(std::function<RawCallbackFn> Callback)
{
  ExternalEventProcessor = std::move(Callback);
}

void Client::exit(ExitReason E, int ECode, std::string Message)
{
  if (Exit != None)
    return;

  LOG(trace) << "Exit with reason " << E << ' ' << ECode << ' ' << Message;
  Exit = E;
  ExitCode = ECode;
  ExitMessage = std::move(Message);
  Poll.reset();
  TerminateLoop.get().store(true);
}

std::size_t Client::consumeNonce() noexcept
{
  assert(Nonce.has_value());
  auto R = *Nonce;
  Nonce.reset();
  return R;
}

std::optional<std::vector<SessionData>> Client::requestSessionList()
{
  using namespace monomux::message;
  auto X = inhibitControlResponse();

  sendMessage(*ControlSocket, request::SessionList{});

  std::optional<response::SessionList> Resp =
    receiveMessage<response::SessionList>(*ControlSocket);
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
Client::requestMakeSession(std::string Name, system::Process::SpawnOptions Opts)
{
  using namespace monomux::message;
  auto X = inhibitControlResponse();

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
  sendMessage(*ControlSocket, Msg);

  std::optional<response::MakeSession> Resp =
    receiveMessage<response::MakeSession>(*ControlSocket);
  if (!Resp || !Resp->Success)
    return std::nullopt;

  return std::move(Resp->Name);
}

bool Client::requestAttach(std::string SessionName)
{
  using namespace monomux::message;
  auto X = inhibitControlResponse();

  request::Attach Msg;
  Msg.Name = std::move(SessionName);
  sendMessage(*ControlSocket, Msg);

  std::optional<response::Attach> Resp =
    receiveMessage<response::Attach>(*ControlSocket);
  if (!Resp)
    Attached = false;
  else
    Attached = Resp->Success;

  if (Attached)
  {
    if (!AttachedSession)
      AttachedSession.emplace();

    AttachedSession->Name = std::move(Resp->Session.Name);
    AttachedSession->Created =
      std::chrono::system_clock::from_time_t(std::move(Resp->Session.Created));
  }

  return Attached;
}

void Client::sendData(std::string_view Data)
{
  if (!DataSocket)
  {
    LOG(error) << "Trying to sendData() but the connection was not established";
    return;
  }
  try
  {
    DataSocket->write(Data);
  }
  catch (const system::BufferedChannel::OverflowError& BO)
  {
    // Allow reschedule later.
  }

  if (DataSocket->hasBufferedWrite())
    Poll->schedule(
      DataSocket->raw(), /* Incoming =*/false, /* Outgoing =*/true);
}

void Client::sendSignal(int Signal)
{
  using namespace monomux::message;
  auto X = inhibitControlResponse();
  request::Signal M;
  M.SigNum = Signal;
  sendMessage(*ControlSocket, M);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Client::notifyWindowSize(unsigned short Rows, unsigned short Columns)
{
  using namespace monomux::message;
  auto X = inhibitControlResponse();
  notification::Redraw M;
  M.Rows = Rows;
  M.Columns = Columns;
  sendMessage(*ControlSocket, M);
}

void Client::enableControlResponse()
{
  if (!Poll)
    return;
  Poll->listen(ControlSocket->raw(), /* Incoming =*/true, /* Outgoing =*/false);
}

void Client::disableControlResponse()
{
  if (!Poll)
    return;
  Poll->stop(ControlSocket->raw());
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

void Client::enableInputFile()
{
  if (!Poll || !system::Handle::isValid(InputFile))
    return;
  Poll->listen(InputFile, /* Incoming =*/true, /* Outgoing =*/false);
  InputFileEnabled = true;
}

void Client::disableInputFile()
{
  if (!Poll || !system::Handle::isValid(InputFile))
    return;
  Poll->stop(InputFile);
  InputFileEnabled = false;
}

Client::Inhibitor Client::inhibitControlResponse()
{
  return Inhibitor{[this] { disableControlResponse(); },
                   [this] { enableControlResponse(); }};
}

Client::Inhibitor Client::inhibitDataSocket()
{
  return Inhibitor{[this] { disableDataSocket(); },
                   [this] { enableDataSocket(); }};
}

Client::Inhibitor Client::inhibitInputFile()
{
  return Inhibitor{[this] { disableInputFile(); },
                   [this] { enableInputFile(); }};
}

} // namespace monomux::client

#undef LOG
