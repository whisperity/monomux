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
#include <thread>

#include <iostream>
#include <system_error>

#include <unistd.h>

using namespace monomux;

namespace monomux {

enum class ForkResult { Error, Parent, Child };

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
      throw std::system_error{std::make_error_code(std::errc::invalid_argument),
                              "Unreachable."};
    case 0:
      // Running in the child.
      Child();
      return ForkResult::Child;
    default:
      Parent();
      return ForkResult::Parent;
  }
}

template <typename Fn> static void forkAndSpecialInParent(Fn&& F)
{
  fork(F, [] { return; });
}

template <typename Fn> static void forkAndSpecialInChild(Fn&& F)
{
  fork([] { return; }, F);
}

} // namespace monomux

#include <sys/wait.h>

int main(int ArgC, char* ArgV[])
{
  // Check if we are a server. If we are a server, we need to do different
  // things.
  if (Server::currentProcessMarkedAsServer())
  {
    std::cout << "MONOMUX SERVER READY AND WAITING." << std::endl;
    CheckedPOSIXThrow([] { return ::daemon(0, 0); }, "Backgrounding ourselves failed", -1);
    std::this_thread::sleep_for(std::chrono::seconds(180));
    std::cout << "Server quit." << std::endl;
    return 0;
  }

  // If we aren't obviously a server, first, always try to establish connection
  // to a server, or if no server is present, assume that the user is trying to
  // start us for the first time, and initialise a server.
  std::optional<ServerConnection> ToServer =
    ServerConnection::create(Server::getServerSocketPath());
  if (!ToServer) {
    forkAndSpecialInChild([&ArgV] {
      Process::SpawnOptions SO;
      SO.Program = ArgV[0];
      SO.Arguments.emplace_back("--server");
      SO.Environment["MONOMUX_SERVER"] = "YES";

      Process::exec(SO);
    });
  }


  // Trash code:
  Pty P;
  {
    Socket S = Socket::create("foo.sock");
    std::cout << "Socket alive\n";
  }
  std::cout << "Socket dead\n";

  int temp;
  std::cout << "Waiting..." << std::endl;
  std::cin >> temp;
  std::cout << "Client exit." << std::endl;

  return temp;
}
