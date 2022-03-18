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
#include "system/Signal.hpp"
#include "system/unreachable.hpp"

#include <iostream>
#include <thread>

namespace monomux
{
namespace server
{

static void serverShutdown(SignalHandling::Signal /* SigNum */,
                           ::siginfo_t* /* Info */,
                           const SignalHandling* Handling)
{
  std::clog << "INFO: Server shutting down..." << std::endl;
  const auto* Srv = std::any_cast<Server*>(Handling->getObject("Server"));
  if (!Srv)
    return;
  (*Srv)->interrupt();
}

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

  {
    SignalHandling& Sig = SignalHandling::get();
    Sig.registerObject("Server", &S);
    Sig.registerCallback(SIGINT, &serverShutdown);
    Sig.registerCallback(SIGTERM, &serverShutdown);
    Sig.enable();
  }

  std::cout << "INFO: Monomux Server starting to listen..." << std::endl;
  S.listen();
  std::cout << "INFO: Server listen exited" << std::endl;

  SignalHandling::get().disable();

  std::cout << "INFO: Server shut down..." << std::endl;
  return 0;
}

} // namespace server
} // namespace monomux
