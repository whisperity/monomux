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
#include "Server.hpp"

#include "control/Message.hpp"
#include "control/Messaging.hpp"
#include "system/CheckedPOSIX.hpp"
#include "system/POD.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include <sys/socket.h>

namespace monomux
{
namespace server
{

Server::Server(Socket&& Sock) : Sock(std::move(Sock))
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

void Server::loop()
{
  static constexpr std::size_t ListenQueue = 16;
  static constexpr std::size_t EventQueue = 1 << 16;
  Sock.listen(ListenQueue);

  POD<struct ::sockaddr_storage> SocketAddr;
  POD<::socklen_t> SocketLen;

  fd::addStatusFlag(Sock.raw(), O_NONBLOCK);
  Poll = std::make_unique<EPoll>(EventQueue);
  Poll->listen(Sock.raw(), /* Incoming =*/true, /* Outgoing =*/false);

  while (!TerminateListenLoop.load())
  {
    reapDeadChildren();

    const std::size_t NumTriggeredFDs = Poll->wait();
    std::clog << "DEBUG: Server - " << NumTriggeredFDs << " events received!"
              << std::endl;
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      if (Poll->fdAt(I) == Sock.raw())
      {
        // Event occured on the main socket.
        std::error_code Error;
        bool Recoverable;
        std::optional<Socket> ClientSock = Sock.accept(&Error, &Recoverable);
        if (!ClientSock)
        {
          std::cerr << "SERVER - accept() did not succeed: " << Error.message()
                    << " - recoverable? " << Recoverable << std::endl;

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
          std::clog << "DEBUG: Server - Stale socket " << ClientSock->raw()
                    << " left behind!" << std::endl;
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
        raw_fd FD = Poll->fdAt(I);
        std::clog << "DEBUG: Server - Data on file descriptor " << FD
                  << std::endl;

        LookupVariant* Entity = FDLookup.tryGet(FD);
        if (!Entity)
        {
          std::clog << "DEBUG: File descriptor " << FD
                    << " not in the lookup table!" << std::endl;
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

void Server::interrupt() const noexcept { TerminateListenLoop.store(true); }

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
  // TODO: Tell clients that the session has gone.

  Sessions.erase(Session.name());
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
  std::cout << "Client connected as " << Client.id() << std::endl;

  raw_fd FD = Client.getControlSocket().raw();

  // (8 is a good guesstimate because FDLookup usually counts from 5 or 6, not
  // from 0.)
  static constexpr std::size_t FDKeepSpare = 8;
  if (FDLookup.size() >= fd::maxNumFDs() - FDKeepSpare)
  {
    // As a full client connection would require *TWO* file descriptors (control
    // and data socket) and we would need to keep 1 open so we can always
    // accept() a connection, reject the client if there aren't any space left.
    std::cerr << "TRACE: Client connected, but there are less than "
                 "satisfactory free file descriptors left. Rejecting..."
              << std::endl;
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

  Socket& ClientSock = Client.getControlSocket();
  std::cout << "Client " << Client.id() << " has data!" << std::endl;

  std::string Data;
  try
  {
    Data = readPascalString(ClientSock);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when reading control from Client " << Client.id()
              << ": " << Err.what() << std::endl;
  }

  if (ClientSock.failed())
  {
    // We realise the client disconnected during an attempt to read.
    exitCallback(Client);
    return;
  }

  if (Data.empty())
    return;

  Client.activity();
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
    Action->second(Client, MB.RawData);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when handling message of client " << Client.id()
              << std::endl;
    if (ClientSock.failed())
      exitCallback(Client);
    return;
  }
}

void Server::dataCallback(ClientData& Client)
{
  // NOLINTNEXTLINE(readability-identifier-naming)
  static constexpr std::size_t BUFFER_SIZE = 1024;

  std::cout << "Client " << Client.id()
            << " sent some data on their data connection!" << std::endl;

  std::string Data;
  try
  {
    Data = Client.getDataSocket()->read(BUFFER_SIZE);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when reading data from "
              << Client.getDataSocket()->raw() << ": " << Err.what()
              << std::endl;
    return;
  }
  std::cout << "DEBUG: Client data received:\n\t" << Data << std::endl;
  Client.activity();

  if (SessionData* S = Client.getAttachedSession())
    S->sendInput(Data);
}

void Server::exitCallback(ClientData& Client)
{
  std::cout << "Client " << Client.id() << " is leaving..." << std::endl;

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
  std::cout << "Session " << Session.name() << " created." << std::endl;

  if (Session.hasProcess() && Session.getProcess().hasPty())
  {
    raw_fd FD = Session.getProcess().getPty()->raw();

    Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
    FDLookup[FD] = SessionConnection{&Session};
  }
}

void Server::dataCallback(SessionData& Session)
{
  // NOLINTNEXTLINE(readability-identifier-naming)
  static constexpr std::size_t BUFFER_SIZE = 1024;

  std::cout << "Session " << Session.name()
            << " sent some data on their data connection!" << std::endl;

  std::string Data;

  try
  {
    Data = Session.readOutput(BUFFER_SIZE);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when reading data from session "
              << Session.getProcess().getPty()->raw() << ": " << Err.what()
              << std::endl;
    return;
  }
  std::cout << "DEBUG: Session data received:\n\t" << Data << std::endl;

  Session.activity();
  for (ClientData* C : Session.getAttachedClients())
    if (Socket* DS = C->getDataSocket())
      try
      {
        DS->write(Data);
      }
      catch (const std::system_error& Err)
      {
        std::cerr << "Error when writing data to client " << C->id() << ": "
                  << Err.what() << std::endl;
      }
}

void Server::clientAttachedCallback(ClientData& Client, SessionData& Session)
{
  std::clog << "DEBUG: Client " << Client.id() << " attached to "
            << Session.name() << std::endl;
  Client.attachToSession(Session);
  Session.attachClient(Client);
}

void Server::clientDetachedCallback(ClientData& Client, SessionData& Session)
{
  std::clog << "DEBUG: Client " << Client.id() << " detached from "
            << Session.name() << std::endl;
  if (Client.getAttachedSession() != &Session)
    return;
  Client.detachSession();
  Session.removeClient(Client);
}

void Server::destroyCallback(SessionData& Session)
{
  std::cout << "Session " << Session.name() << " exited..." << std::endl;

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
  std::clog << "DEBUG: Client " << DataClient.id()
            << " becoming data connection for " << MainClient.id() << std::endl;
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

    bool Dead = SessionForProc->second->getProcess().reapIfDead();
    if (Dead)
    {
      std::clog << "INFO: Child process #" << PID << " of session '"
                << SessionForProc->second->name() << "' exited." << std::endl;

      destroyCallback(*SessionForProc->second);
    }

    PID = Process::Invalid;
  }
}

} // namespace server
} // namespace monomux
