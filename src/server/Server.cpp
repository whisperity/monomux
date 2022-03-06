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
          // Start monitoring the accepted connection/socket.
          raw_fd ClientFD = Established.get();

          auto ExistingIt = Clients.find(ClientFD);
          if (ExistingIt != Clients.end())
          {
            // The client with the same socket FD is already known.
            // TODO: What is the good way of handling this?
            std::clog << "DEBUG: Server - Stale socket " << ClientFD
                      << " left behind!" << std::endl;
            exitCallback(ExistingIt->second);
            Poll->stop(ClientFD);
            Clients.erase(ExistingIt);
          }

          fd::setNonBlockingCloseOnExec(ClientFD);
          Poll->listen(ClientFD, /* Incoming =*/true, /* Outgoing =*/false);
          Clients.emplace(ClientFD,
                          std::make_unique<Socket>(Socket::wrap(fd(ClientFD))));
          acceptCallback(Clients.at(ClientFD));
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
    Poll->stop(ClientSock.raw());
    Clients.erase(ClientSock.raw());
    return;
  }

  std::cout << Data << std::endl;

  std::cout << "Check for message kind... ";

  MessageKind MK = kindFromStr(Data);
  auto Action = Dispatch.find(static_cast<decltype(Dispatch)::key_type>(MK));
  if (Action == Dispatch.end())
  {
    std::cerr << "Error: Unknown message type " << static_cast<int>(MK)
              << " received." << std::endl;
    return;
  }
  std::cout << static_cast<int>(MK) << std::endl;

  std::cout << "Read data " << std::string_view{Data.data(), Data.size()}
            << std::endl;
  Action->second(
    Client,
    std::string_view{Data.data() + sizeof(MK), Data.size() - sizeof(MK)});
}

void Server::exitCallback(ClientData& Client)
{
  std::cout << "Client " << Client.getControlSocket().raw() << " is leaving..."
            << std::endl;
}

Server::ClientData::ClientData(std::unique_ptr<Socket> Connection)
  : ID(Connection->raw()), ControlConnection(std::move(Connection))
{}

Server::ClientData::~ClientData() = default;

static std::size_t NonceCounter = 0; // FIXME: Remove this.

std::size_t Server::ClientData::makeNewNonce() noexcept
{
  // FIXME: Better random number generation.
  Nonce = ++NonceCounter;
  return Nonce;
}

} // namespace monomux
