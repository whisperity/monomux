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
#include <chrono>
#include <thread>

#include "monomux/adt/POD.hpp"
#include "monomux/control/PascalString.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/server/Server.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("server/Server")

namespace monomux::server
{

Server::Server(Socket&& Sock)
  : Sock(std::move(Sock)), ExitIfNoMoreSessions(false)
{
  setUpDispatch();
  DeadChildren.fill(Process::Invalid);
}

Server::~Server() = default;

void Server::registerMessageHandler(std::uint16_t Kind,
                                    std::function<HandlerFunction> Handler)
{
  Dispatch[Kind] = std::move(Handler);
}

void Server::setExitIfNoMoreSessions(bool ExitIfNoMoreSessions)
{
  this->ExitIfNoMoreSessions = ExitIfNoMoreSessions;
}

void Server::loop()
{
  static constexpr std::size_t ListenQueue = 16;
  static constexpr std::size_t EventQueue = 1 << 16;
  Sock.listen(ListenQueue);

  fd::addStatusFlag(Sock.raw(), O_NONBLOCK);
  Poll = std::make_unique<EPoll>(EventQueue);
  Poll->listen(Sock.raw(), /* Incoming =*/true, /* Outgoing =*/false);

  while (!TerminateLoop.get().load())
  {
    // Process "external" events.
    reapDeadChildren();

    const std::size_t NumTriggeredFDs = Poll->wait();
    MONOMUX_TRACE_LOG(LOG(data) << NumTriggeredFDs << " events received!");
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      raw_fd EventFD = Poll->fdAt(I);
      if (EventFD == fd::Invalid)
      {
        LOG(error) << '#' << I
                   << " event received but there was no associated file";
        continue;
      }

      if (EventFD == Sock.raw())
      {
        // Event occured on the main socket.
        std::error_code Error;
        bool Recoverable;
        std::optional<Socket> ClientSock = Sock.accept(&Error, &Recoverable);
        if (!ClientSock)
        {
          if (Recoverable)
            LOG(warn) << "accept() did not succeed: " << Error;
          else
            LOG(error) << "accept() did not succeed: " << Error
                       << " (not recoverable)";

          if (Recoverable)
          {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            --I;
          }
          continue;
        }

        // A new client was accepted.
        if (ClientData* ExistingClient = getClient(ClientSock->raw()))
        {
          // The client with the same socket FD is already known.
          // TODO: What is the good way of handling this?
          LOG(debug) << "Stale socket of gone client, " << ClientSock->raw()
                     << " left behind?";
          exitCallback(*ExistingClient);
          removeClient(*ExistingClient);
        }

        ClientData* Client = makeClient(
          ClientData{std::make_unique<Socket>(std::move(*ClientSock))});
        acceptCallback(*Client);
      }
      else
      {
        // Event occured on another (connected client) socket.
        MONOMUX_TRACE_LOG(LOG(trace) << "Data on file descriptor " << EventFD);

        LookupVariant* Entity = FDLookup.tryGet(EventFD);
        if (!Entity)
        {
          LOG(error) << "\tFile descriptor " << EventFD
                     << " missing from lookup table? (Possible internal error "
                        "or race condition!)";
          continue;
        }

        if (auto* Session = std::get_if<SessionConnection>(Entity))
        {
          // First check for data coming from a session. This is the most
          // populous in terms of bandwidth.
          dataCallback(**Session);
          continue;
        }
        if (auto* Data = std::get_if<ClientDataConnection>(Entity))
        {
          // Second, try to see if the data is coming from a client, like
          // keypresses and such. We expect to see many of these, too.
          dataCallback(**Data);
          continue;
        }
        if (auto* Control = std::get_if<ClientControlConnection>(Entity))
        {
          // Lastly, check if the receive is happening on the control
          // connection, where messages are small and far inbetween.
          controlCallback(**Control);
          continue;
        }
      }
    }
  }
}

void Server::interrupt() const noexcept { TerminateLoop.get().store(true); }

void Server::shutdown()
{
  LOG(info) << "Detaching all clients...";
  while (!Clients.empty())
  {
    ClientData& Client = *Clients.begin()->second;
    try
    {
      Client.sendDetachReason(
        monomux::message::notification::Detached::ServerShutdown);
    }
    catch (const std::system_error&)
    {
      // Ignore the error. It could be that the client got auto-detacehd when
      // their associated session ended.
    }
    removeClient(Client);
  }

  LOG(info) << "Terminating all sessions...";
  while (!Sessions.empty())
  {
    SessionData& Session = *Sessions.begin()->second;
    removeSession(Session);
  }
}

ClientData* Server::getClient(std::size_t ID) noexcept
{
  auto It = Clients.find(ID);
  return It != Clients.end() ? It->second.get() : nullptr;
}

SessionData* Server::getSession(std::string_view Name) noexcept
{
  auto It =
    std::find_if(Sessions.begin(), Sessions.end(), [Name](const auto& Elem) {
      return Elem.first == Name;
    });
  return It != Sessions.end() ? It->second.get() : nullptr;
}

ClientData* Server::makeClient(ClientData Client)
{
  std::size_t CID = Client.id();
  auto InsertRes =
    Clients.try_emplace(CID, std::make_unique<ClientData>(std::move(Client)));
  if (!InsertRes.second)
    return nullptr;
  return InsertRes.first->second.get();
}

SessionData* Server::makeSession(SessionData Session)
{
  std::string SN = Session.name();
  auto InsertRes =
    Sessions.try_emplace(SN, std::make_unique<SessionData>(std::move(Session)));
  if (!InsertRes.second)
    return nullptr;
  return InsertRes.first->second.get();
}

void Server::removeClient(ClientData& Client)
{
  std::size_t CID = Client.id();
  if (SessionData* S = Client.getAttachedSession())
    clientDetachedCallback(Client, *S);
  Clients.erase(CID);
}

void Server::removeSession(SessionData& Session)
{
  for (ClientData* C : Session.getAttachedClients())
    clientDetachedCallback(*C, Session);

  Sessions.erase(Session.name());

  if (Sessions.empty() && ExitIfNoMoreSessions)
    TerminateLoop.get().store(true);
}

void Server::registerDeadChild(Process::raw_handle PID) const noexcept
{
  for (std::size_t I = 0; I < DeadChildrenVecSize; ++I)
    if (DeadChildren.at(I) == Process::Invalid)
    {
      DeadChildren.at(I) = PID;
      return;
    }
}

void Server::acceptCallback(ClientData& Client)
{
  LOG(info) << "Client \"" << Client.id() << "\" connected";

  raw_fd FD = Client.getControlSocket().raw();

  // (8 is a good guesstimate because FDLookup usually counts from 5 or 6, not
  // from 0.)
  static constexpr std::size_t FDKeepSpare = 8;
  std::size_t FDCount = FDLookup.size();
  std::size_t MaxFDs = fd::maxNumFDs() - FDKeepSpare;
  if (FDCount >= MaxFDs)
  {
    // As a full client connection would require *TWO* file descriptors (control
    // and data socket) and we would need to keep 1 open so we can always
    // accept() a connection, reject the client if there aren't any space left.
    LOG(warn) << "Self-defence rejecting client - " << FDCount
              << " FDs allocated out of the max " << MaxFDs;
    sendRejectClient(Client, "Not enough file descriptors left on server.");
    removeClient(Client);
    return;
  }

  fd::setNonBlockingCloseOnExec(FD);
  Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
  FDLookup[FD] = ClientControlConnection{&Client};

  sendAcceptClient(Client);
}

void Server::controlCallback(ClientData& Client)
{
  using namespace monomux::message;
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << Client.id() << "\" sent CONTROL!");
  Socket& ClientSock = Client.getControlSocket();
  std::string Data;

  // Consume all the control messages that might be on the socket if a burst
  // happened.
  while (true)
  {
    Data.clear();
    try
    {
      Data = readPascalString(ClientSock);
    }
    catch (const std::system_error& Err)
    {
      LOG(error) << "Client \"" << Client.id()
                 << "\": error when reading CONTROL: " << Err.what();
    }

    if (ClientSock.failed())
    {
      // We realise the client disconnected during an attempt to read.
      exitCallback(Client);
      return;
    }

    if (Data.empty())
      return;

    Message MB = Message::unpack(Data);
    auto Action =
      Dispatch.find(static_cast<decltype(Dispatch)::key_type>(MB.Kind));
    if (Action == Dispatch.end())
    {
      MONOMUX_TRACE_LOG(LOG(trace) << "Client \"" << Client.id()
                                   << "\": unknown message type "
                                   << static_cast<int>(MB.Kind) << " received");
      continue;
    }

    MONOMUX_TRACE_LOG(LOG(data) << "Client \"" << Client.id() << "\"\n"
                                << MB.RawData);
    try
    {
      Action->second(*this, Client, MB.RawData);
    }
    catch (const std::system_error& Err)
    {
      LOG(error) << "Client \"" << Client.id()
                 << "\": error when handling message";
      if (ClientSock.failed())
        exitCallback(Client);
      continue;
    }
  }
}

void Server::dataCallback(ClientData& Client)
{
  static constexpr std::size_t BufferSize = 1024;

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << Client.id() << "\" sent DATA!");
  std::string Data;
  try
  {
    Data = Client.getDataSocket()->read(BufferSize);
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Client \"" << Client.id()
               << "\": error when reading DATA: " << Err.what();
    return;
  }

  if (Client.getDataSocket()->failed())
  {
    // We realise the client disconnected during an attempt to read.
    exitCallback(Client);
    return;
  }

  Client.activity();
  MONOMUX_TRACE_LOG(LOG(data)
                    << "Client \"" << Client.id() << "\" data: " << Data);

  if (SessionData* S = Client.getAttachedSession())
    S->sendInput(Data);
}

void Server::exitCallback(ClientData& Client)
{
  LOG(info) << "Client \"" << Client.id() << "\" exited";

  if (const auto* DS = Client.getDataSocket())
  {
    Poll->stop(DS->raw());
    FDLookup.erase(DS->raw());
  }

  Poll->stop(Client.getControlSocket().raw());
  FDLookup.erase(Client.getControlSocket().raw());

  removeClient(Client);
}

void Server::createCallback(SessionData& Session)
{
  LOG(info) << "Session \"" << Session.name() << "\" created";
  if (Session.hasProcess() && Session.getProcess().hasPty())
  {
    raw_fd FD = Session.getProcess().getPty()->raw();

    Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
    FDLookup[FD] = SessionConnection{&Session};
  }
}

void Server::dataCallback(SessionData& Session)
{
  static constexpr std::size_t BufferSize = 1024;

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Session \"" << Session.name() << "\" sent DATA!");
  std::string Data;
  try
  {
    Data = Session.readOutput(BufferSize);
  }
  catch (const std::system_error& Err)
  {
    LOG(error) << "Session \"" << Session.name()
               << "\": error when reading DATA: " << Err.what();
    return;
  }

  Session.activity();
  MONOMUX_TRACE_LOG(LOG(data)
                    << "Session \"" << Session.name() << "\" data: " << Data);

  for (ClientData* C : Session.getAttachedClients())
    if (Socket* DS = C->getDataSocket())
      try
      {
        DS->write(Data);
      }
      catch (const std::system_error& Err)
      {
        LOG(error) << "Session \"" << Session.name()
                   << "\": error when sending DATA to attached client \""
                   << C->id() << "\": " << Err.what();

        if (DS->failed())
        {
          // We realise the client disconnected during an attempt to send.
          exitCallback(*C);
          continue;
        }
      }
}

void Server::clientAttachedCallback(ClientData& Client, SessionData& Session)
{
  LOG(info) << "Client \"" << Client.id() << "\" attached to \""
            << Session.name() << '"';
  Client.attachToSession(Session);
  Session.attachClient(Client);
}

void Server::clientDetachedCallback(ClientData& Client, SessionData& Session)
{
  if (Client.getAttachedSession() != &Session)
    return;
  LOG(info) << "Client \"" << Client.id() << "\" detached from \""
            << Session.name() << '"';
  Client.detachSession();
  Session.removeClient(Client);
}

void Server::destroyCallback(SessionData& Session)
{
  LOG(info) << "Session \"" << Session.name() << "\" exited";
  if (Session.hasProcess() && Session.getProcess().hasPty())
  {
    raw_fd FD = Session.getProcess().getPty()->raw();

    Poll->stop(FD);
    FDLookup.erase(FD);
  }

  removeSession(Session);
}

void Server::turnClientIntoDataOfOtherClient(ClientData& MainClient,
                                             ClientData& DataClient)
{
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Client \"" << DataClient.id()
                    << "\" becoming the DATA connection for Client \""
                    << MainClient.id() << '"');
  MainClient.subjugateIntoDataSocket(DataClient);
  FDLookup[MainClient.getDataSocket()->raw()] =
    ClientDataConnection{&MainClient};

  // Remove the object from the owning data structure but do not fire the exit
  // handler!
  Clients.erase(DataClient.id());
}

void Server::reapDeadChildren()
{
  for (Process::raw_handle& PID : DeadChildren)
  {
    if (PID == Process::Invalid)
      continue;

    auto SessionForProc =
      std::find_if(Sessions.begin(), Sessions.end(), [PID](auto&& E) {
        return E.second->hasProcess() && E.second->getProcess().raw() == PID;
      });
    if (SessionForProc == Sessions.end())
      continue;
    Process& Proc = SessionForProc->second->getProcess();

    bool Dead = Proc.reapIfDead();
    if (Dead)
    {
      LOG(debug) << "Child PID " << PID << " of Session \""
                 << SessionForProc->second->name() << "\" exited with "
                 << Proc.exitCode();

      for (ClientData* AC : SessionForProc->second->getAttachedClients())
        AC->sendDetachReason(monomux::message::notification::Detached::Exit,
                             Proc.exitCode());
      destroyCallback(*SessionForProc->second);
    }

    PID = Process::Invalid;
  }
}

} // namespace monomux::server

#undef LOG
