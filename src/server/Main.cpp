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

#include "ExitCode.hpp"
#include "Server.hpp"
#include "system/Environment.hpp"
#include "system/Process.hpp"
#include "system/Signal.hpp"
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
  if (SocketPath.has_value())
  {
    Ret.emplace_back("--socket");
    Ret.emplace_back(*SocketPath);
  }

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

/// Handler for request to terinate the server.
static void serverShutdown(SignalHandling::Signal /* SigNum */,
                           ::siginfo_t* /* Info */,
                           const SignalHandling* Handling)
{
  const volatile auto* Srv =
    std::any_cast<Server*>(Handling->getObject("Server"));
  if (!Srv)
    return;
  (*Srv)->interrupt();
}

/// Handler for \p SIGCHLD when a process spawned by the server quits.
static void childExited(SignalHandling::Signal /* SigNum */,
                        ::siginfo_t* Info,
                        const SignalHandling* Handling)
{
  Process::raw_handle CPID = Info->si_pid;
  const volatile auto* Srv =
    std::any_cast<Server*>(Handling->getObject("Server"));
  if (!Srv)
    return;
  (*Srv)->registerDeadChild(CPID);
}

int main(Options& Opts)
{
  // CheckedPOSIXThrow([] { return ::daemon(0, 0); }, "Backgrounding ourselves
  // failed", -1);

  Socket ServerSock = Socket::create(*Opts.SocketPath);
  Server S = Server(std::move(ServerSock));

  {
    SignalHandling& Sig = SignalHandling::get();
    Sig.registerObject("Server", &S);
    Sig.registerCallback(SIGINT, &serverShutdown);
    Sig.registerCallback(SIGTERM, &serverShutdown);
    Sig.registerCallback(SIGCHLD, &childExited);
    Sig.ignore(SIGPIPE);
    Sig.enable();
  }

  std::cout << "INFO: Monomux Server starting to listen..." << std::endl;
  S.loop();
  std::cout << "INFO: Server listen exited" << std::endl;

  SignalHandling::get().disable();

  S.shutdown();

  std::cout << "INFO: Server shut down..." << std::endl;
  return EXIT_Success;
}

} // namespace server
} // namespace monomux
