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
#include "system/CheckedPOSIX.hpp"
#include "system/POD.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include <sys/socket.h>

namespace monomux
{

std::string Server::getServerSocketPath()
{
  // TODO: Handle XDG_RUNTIME_DIR, etc.
  return "monomux.server.sock";
}

Server::Server(Socket&& Sock) : Sock(std::move(Sock)) { setUpDispatch(); }

Server::~Server()
{
  TerminateListenLoop.store(true);
  // TODO: Wake the poller so the loop can gracefully exit.
}

int Server::listen()
{
  Sock.listen();

  POD<struct ::sockaddr_storage> SocketAddr;
  POD<::socklen_t> SocketLen;

  fd::addStatusFlag(Sock.raw(), O_NONBLOCK);
  Poll = std::make_unique<EPoll>(8);
  Poll->listen(Sock.raw(), /* Incoming =*/true, /* Outgoing =*/false);

  while (TerminateListenLoop.load() == false)
  {
    const std::size_t NumTriggeredFDs = Poll->wait();
    std::clog << "DEBUG: Server - " << NumTriggeredFDs << " events received!"
              << std::endl;
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      if (Poll->fdAt(I) == Sock.raw())
      {
        // Event occured on the main socket.
        auto Established = CheckedPOSIX(
          [this, &SocketAddr, &SocketLen] {
            return ::accept(Sock.raw(),
                            reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                            &SocketLen);
          },
          -1);
        if (!Established)
        {
          std::errc EC = static_cast<std::errc>(Established.getError().value());
          if (EC == std::errc::resource_unavailable_try_again /* EAGAIN */ ||
              EC == std::errc::interrupted /* EINTR */ ||
              EC == std::errc::connection_aborted /* ECONNABORTED */)
          {
            std::cerr << "accept() " << std::make_error_code(EC) << std::endl;
            continue;
          }
          else if (EC == std::errc::too_many_files_open /* EMFILE */ ||
                   EC == std::errc::too_many_files_open_in_system /* ENFILE */)
          {
            std::cerr << "accept() " << std::make_error_code(EC) << ", sleep..."
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
          else
            std::cerr << "accept() failed: " << Established.getError()
                      << std::endl;
        }
        else
        {
          // A new client was accepted.
          raw_fd ClientFD = Established.get();

          auto ExistingIt = Clients.find(ClientFD);
          if (ExistingIt != Clients.end())
          {
            // The client with the same socket FD is already known.
            // TODO: What is the good way of handling this?
            std::clog << "DEBUG: Server - Stale socket " << ClientFD
                      << " left behind!" << std::endl;
            exitCallback(ExistingIt->second);
          }

          auto InsertResult = Clients.emplace(
            ClientFD, std::make_unique<Socket>(Socket::wrap(fd(ClientFD))));
          acceptCallback(InsertResult.first->second);
        }
      }
      else
      {
        // Event occured on another (connected client) socket.
        raw_fd ClientFD = Poll->fdAt(I);
        std::clog << "DEBUG: Server - Data on client " << ClientFD << std::endl;

        auto It = Clients.find(ClientFD);
        if (It != Clients.end())
          readCallback(It->second);
      }
    }
  }

  return 0;
}

void Server::acceptCallback(ClientData& Client)
{
  std::cout << "Client connected " << Client.getControlSocket().raw()
            << std::endl;

  raw_fd FD = Client.getControlSocket().raw();

  fd::setNonBlockingCloseOnExec(FD);
  Poll->listen(FD, /* Incoming =*/true, /* Outgoing =*/false);
}

void Server::readCallback(ClientData& Client)
{
  Socket& ClientSock = Client.getControlSocket();
  std::cout << "Client " << ClientSock.raw() << " has data!" << std::endl;

  std::string Data;
  try
  {
    Data = ClientSock.read(1024);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when reading data from " << ClientSock.raw() << ": "
              << Err.what() << std::endl;
    return;
  }

  if (!ClientSock.believeConnectionOpen())
  {
    // We realise the client disconnected during an attempt to read.
    exitCallback(Client);
    return;
  }

  std::cout << Data << std::endl;

  std::cout << "Check for message kind... ";

  MessageBase MB = MessageBase::unpack(Data);
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
  Action->second(Client, MB.RawData);
}

void Server::exitCallback(ClientData& Client)
{
  std::cout << "Client " << Client.getControlSocket().raw() << " is leaving..."
            << std::endl;

  raw_fd ClientFD = Client.getControlSocket().raw();
  Poll->stop(Client.getControlSocket().raw());
  auto It = Clients.find(ClientFD);
  if (It != Clients.end())
    Clients.erase(It);
}

Server::ClientData::ClientData(std::unique_ptr<Socket> Connection)
  : ID(Connection->raw()), ControlConnection(std::move(Connection))
{}

Server::ClientData::~ClientData() = default;

static std::size_t NonceCounter = 0; // FIXME: Remove this.

std::size_t Server::ClientData::makeNewNonce() noexcept
{
  // FIXME: Better random number generation.
  Nonce.emplace(++NonceCounter);
  return *Nonce;
}

} // namespace monomux
