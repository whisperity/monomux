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
#include "Main.hpp"

#include "Server.hpp"
#include "system/Process.hpp"
#include "system/unreachable.hpp"

#include <iostream>
#include <thread>

namespace monomux
{

namespace server
{

std::vector<std::string> Options::toArgv() const
{
  std::vector<std::string> Ret;

  if (ServerMode)
    Ret.emplace_back("--server");

  return Ret;
}

[[noreturn]] void exec(const Options& Opts, const char* ArgV0)
{
  std::clog << "DEBUG: exec() a new server!" << std::endl;

  Process::SpawnOptions SO;
  SO.Program = ArgV0;
  SO.Arguments = Opts.toArgv();

  Process::exec(SO);
  unreachable("[[noreturn]]");
}

int main(Options& Opts)
{
  // Server::consumeProcessMarkedAsServer();
  // CheckedPOSIXThrow([] { return ::daemon(0, 0); }, "Backgrounding ourselves
  // failed", -1);

  Socket ServerSock = Socket::create(Server::getServerSocketPath());
  Server S = Server(std::move(ServerSock));
  // TODO: Signal handler! If interrupted, the cleanup does not happen here.

  std::cout << "INFO: Monomux Server starting to listen..." << std::endl;
  int R = S.listen();
  std::cout << "INFO: Server listen exited with " << R << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(30));

  std::cout << "INFO: Server shut down..." << std::endl;
  return R;
}

} // namespace server
} // namespace monomux
