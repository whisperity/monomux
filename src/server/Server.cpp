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
#include "control/SocketMessaging.hpp"
#include "system/Environment.hpp"
#include "system/POD.hpp"
#include "system/Process.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <thread>
#include <utility>
#include <vector>

#include <sys/epoll.h>
#include <sys/socket.h>

namespace monomux
{

namespace detail
{

/// Wrapper over an epoll(7) structure.
class EPoll
{
private:
  raw_fd ListenSocket; // Non-owning.
  fd EPollMasterFD;
  POD<struct ::epoll_event> MainEvent;      // Used to register events.
  std::vector<struct ::epoll_event> Events; // Contains events that fired.

  friend class EPollListener;

  /// RAII object that manages assigning a socket into the epoll(7) structure.
  class EPollListener
  {
  private:
    EPoll& Master;
    raw_fd SocketToListen;

  public:
    EPollListener(EPoll& Master, raw_fd Socket)
      : Master(Master), SocketToListen(Socket)
    {
      Master.MainEvent->data.fd = Socket;
      Master.MainEvent->events = EPOLLIN | EPOLLET;
      CheckedPOSIXThrow(
        [&Master, Socket] {
          return ::epoll_ctl(
            Master.EPollMasterFD, EPOLL_CTL_ADD, Socket, &Master.MainEvent);
        },
        "epoll_ctl registering client socket",
        -1);
      std::clog << "Start listening for " << SocketToListen << std::endl;
    }

    ~EPollListener()
    {
      if (SocketToListen == InvalidFD)
        return;

      std::clog << "No longer listening on " << SocketToListen << std::endl;
      CheckedPOSIX(
        [this] {
          return ::epoll_ctl(Master.EPollMasterFD,
                             EPOLL_CTL_DEL,
                             SocketToListen,
                             &Master.MainEvent);
        },
        -1);
    }

    EPollListener(EPollListener&& RHS)
      : Master(RHS.Master), SocketToListen(RHS.SocketToListen)
    {
      RHS.SocketToListen = InvalidFD;
    }
  };

  std::map<raw_fd, EPollListener> Listeners;

public:
  std::size_t maxEventCount() const { return Events.size(); }

  EPoll(raw_fd ListenSocket, std::size_t EventCount)
  {
    // Create a file descriptor to be monitored by EPoll.
    EPollMasterFD =
      CheckedPOSIXThrow([EventCount] { return ::epoll_create(EventCount); },
                        "epoll_create()",
                        -1);
    setNonBlockingCloseOnExec(EPollMasterFD.get());
    Events.resize(EventCount);

    // Register the main socket into the event poller.
    MainEvent->data.fd = ListenSocket;
    MainEvent->events = EPOLLIN | EPOLLET;
    CheckedPOSIXThrow(
      [this, ListenSocket] {
        return ::epoll_ctl(
          EPollMasterFD, EPOLL_CTL_ADD, ListenSocket, &MainEvent);
      },
      "epoll_ctl registering main",
      -1);
  }

  /// Waits for events to occur. Returns the number of file descriptors
  /// affected.
  std::size_t wait()
  {
    return CheckedPOSIXThrow(
      [this] {
        return ::epoll_wait(EPollMasterFD, Events.data(), Events.size(), -1);
      },
      "epoll_wait",
      -1);
  }

  struct ::epoll_event& operator[](std::size_t Index)
  {
    return Events.at(Index);
  }

  void registerListenOn(raw_fd SocketFD)
  {
    Listeners.try_emplace(SocketFD, EPollListener{*this, SocketFD});
  }

  void stopListenOn(raw_fd SocketFD)
  {
    auto It = Listeners.find(SocketFD);
    if (It == Listeners.end())
      return;
    Listeners.erase(It);
  }
};

} // namespace detail

bool Server::currentProcessMarkedAsServer() noexcept
{
  return getEnv("MONOMUX_SERVER") == "YES";
}

void Server::consumeProcessMarkedAsServer() noexcept
{
  ::unsetenv("MONOMUX_SERVER");
}

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
  if (Poll)
    delete Poll;
}

int Server::listen()
{
  Sock.listen();

  POD<struct ::sockaddr_storage> SocketAddr;
  POD<::socklen_t> SocketLen;

  addStatusFlag(Sock.raw(), O_NONBLOCK);
  Poll = new detail::EPoll{Sock.raw(), 16};

  while (TerminateListenLoop.load() == false)
  {
    const std::size_t NumTriggeredFDs = Poll->wait();
    std::clog << NumTriggeredFDs << " events received!" << std::endl;
    for (std::size_t I = 0; I < NumTriggeredFDs; ++I)
    {
      if ((*Poll)[I].data.fd == Sock.raw())
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
          raw_fd Client = Established.get();

          auto ExistingIt = ClientSockets.find(Client);
          if (ExistingIt != ClientSockets.end())
          {
            // The client with the same socket FD is already known.
            // TODO: What is the good way of handling this?
            std::clog << "Stale socket " << Client << " left behind!"
                      << std::endl;
            exitCallback(*ExistingIt->second);
            Poll->stopListenOn(Client);
            ClientSockets.erase(ExistingIt);
          }

          setNonBlockingCloseOnExec(Client);
          Poll->registerListenOn(Client);
          ClientSockets[Client] =
            std::make_unique<Socket>(Socket::wrap(fd(Client)));
          acceptCallback(*ClientSockets[Client]);
        }
      }
      else
      {
        // Event occured on another (connected client) socket.
        raw_fd Client = (*Poll)[I].data.fd;
        std::cout << "Data on client " << Client << std::endl;

        auto It = ClientSockets.find(Client);
        if (It != ClientSockets.end())
          readCallback(*It->second);
      }
    }
  }

  return 0;
}

void Server::acceptCallback(Socket& Client)
{
  std::cout << "Client connected " << Client.raw() << std::endl;
}

void Server::readCallback(Socket& Client)
{
  std::cout << "Client " << Client.raw() << " has data!" << std::endl;

  std::string Data;
  try
  {
    Data = Client.read(1024);
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "Error when reading data from " << Client.raw() << ": "
              << Err.what() << std::endl;
    return;
  }

  if (!Client.believeConnectionOpen())
  {
    // We realise the client disconnected during an attempt to read.
    exitCallback(Client);
    if (Poll)
      Poll->stopListenOn(Client.raw());
    ClientSockets.erase(Client.raw());
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
  Action->second(Client, std::move(Data));
}

void Server::exitCallback(Socket& Client)
{
  std::cout << "Client " << Client.raw() << " is leaving..." << std::endl;
}

void Server::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME)                                                           \
  std::bind(std::mem_fn(&Server::NAME),                                        \
            this,                                                              \
            std::placeholders::_1,                                             \
            std::placeholders::_2)
#define DISPATCH(K, FUNCTION) Dispatch.try_emplace(KIND(K), MEMBER(FUNCTION));
#include "Server.Dispatch.ipp"
#undef DISPATCH
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME) void Server::NAME(Socket& Client, std::string RawMessage)

HANDLER(clientID)
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;

  auto Msg = request::ClientID::decode(RawMessage);
  if (!Msg)
    return;

  std::cout << "Client #" << Client.raw() << ": Request Client ID" << std::endl;

  response::ClientID Resp;
  Resp.ID = Client.raw();

  writeMessage(Client, std::move(Resp));
}

HANDLER(spawnProcess)
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;

  auto Msg = request::SpawnProcess::decode(RawMessage);
  if (!Msg)
    return;

  std::cout << "Spawn: " << Msg->ProcessName << std::endl;

  Process::SpawnOptions SOpts;
  SOpts.Program = Msg->ProcessName;
  SOpts.CreatePTY = true;

  Process P = Process::spawn(SOpts);
}

#undef HANDLER

} // namespace monomux
