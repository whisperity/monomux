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
#include "CheckedPOSIX.hpp"
#include "Environment.hpp"
#include "Process.hpp"
#include "Pty.hpp"
#include "Server.hpp"
#include "ServerConnection.hpp"

#include <chrono>
#include <iostream>
#include <system_error>
#include <thread>

#include <unistd.h>

using namespace monomux;

namespace monomux
{

enum class ForkResult
{
  Error,
  Parent,
  Child
};

/// Performs \p fork() on the current process and executes the given two actions
/// in the remaining parent and child process.
template <typename ParentFn, typename ChildFn>
static ForkResult fork(ParentFn&& Parent, ChildFn&& Child)
{
  auto ForkResult =
    CheckedPOSIXThrow([] { return ::fork(); }, "fork() failed", -1);
  switch (ForkResult)
  {
    case -1:
      // Fork failed.
      // TODO: Something like llvm_unreachable() which hard-destroys us at hit.
      throw std::runtime_error{"Unreachable."};
    case 0:
      // Running in the child.
      Child();
      return ForkResult::Child;
    default:
      Parent();
      return ForkResult::Parent;
  }
}

/// Shorthand for \p fork() where the child continues execution normally.
template <typename Fn> static void forkAndSpecialInParent(Fn&& F)
{
  fork(F, [] { return; });
}

/// Shorthand for \p fork() where the parent continues execution normally.
template <typename Fn> static void forkAndSpecialInChild(Fn&& F)
{
  fork([] { return; }, F);
}

} // namespace monomux

int serve()
{
  Server::consumeProcessMarkedAsServer();
  // CheckedPOSIXThrow([] { return ::daemon(0, 0); }, "Backgrounding ourselves
  // failed", -1);

  Socket ServerSock = Socket::create(Server::getServerSocketPath());
  Server S = Server(std::move(ServerSock));
  // TODO: Signal handler! If interrupted, the cleanup does not happen here.

  std::cout << "Server socket created, ready!" << std::endl;

  int R = S.listen();
  std::cout << "Server listen exited with " << R << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(30));

  std::cout << "Server shut down..." << std::endl;
  return R;
}

int main(int ArgC, char* ArgV[])
{
  // Check if we are a server. If we are a server, we need to do different
  // things.
  if (Server::currentProcessMarkedAsServer())
    return serve();

  // If we aren't obviously a server, first, always try to establish connection
  // to a server, or if no server is present, assume that the user is trying to
  // start us for the first time, and initialise a server.
  std::optional<ServerConnection> ToServer =
    ServerConnection::create(Server::getServerSocketPath());
  if (!ToServer)
  {
    return -2;
    // Perform the server restart in the child, so it gets disowned when we
    // eventually exit, and we can remain the client.
    forkAndSpecialInChild([&ArgV] {
      std::cerr << "No running server found, creating one..." << std::endl;

      Process::SpawnOptions SO;
      SO.Program = ArgV[0];
      SO.Arguments.emplace_back("--server");
      SO.Environment["MONOMUX_SERVER"] = "YES";

      Process::exec(SO);
    });
  }

  while (!ToServer)
  {
    std::clog << "Trying to connect to server..." << std::endl;
    ToServer = ServerConnection::create(Server::getServerSocketPath());
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "Connection established!" << std::endl;

  if (ArgC >= 2)
  {
    Process::SpawnOptions SO;
    SO.Program = ArgV[1];
    ToServer->requestSpawnProcess(SO);
  }

  // Trash code:
  int temp;
  std::cout << "Waiting..." << std::endl;
  std::cin >> temp;
  std::cout << "Client exit." << std::endl;

  return temp;
}
