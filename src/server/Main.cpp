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
#include <thread>

#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/unreachable.hpp"

#include "Main.hpp"

#include "ExitCode.hpp"
#include "Server.hpp"
#include "system/Environment.hpp"
#include "system/Signal.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("server/Main")

namespace monomux::server
{

Options::Options()
  : ServerMode(false), Background(true), ExitOnLastSessionTerminate(true)
{}

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
  if (!Background)
    Ret.emplace_back("--no-daemon");
  if (!ExitOnLastSessionTerminate)
    Ret.emplace_back("--keepalive");

  return Ret;
}

[[noreturn]] void exec(const Options& Opts, const char* ArgV0)
{
  MONOMUX_TRACE_LOG(LOG(trace) << "exec() a new server");

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
  if (Opts.Background)
    CheckedPOSIXThrow(
      [] { return ::daemon(0, 0); }, "Backgrounding ourselves failed", -1);

  Socket ServerSock = Socket::create(*Opts.SocketPath);
  Server S = Server(std::move(ServerSock));
  S.setExitIfNoMoreSessions(Opts.ExitOnLastSessionTerminate);
  ScopeGuard Signal{[&S] {
                      SignalHandling& Sig = SignalHandling::get();
                      Sig.registerObject("Server", &S);
                      Sig.registerCallback(SIGINT, &serverShutdown);
                      Sig.registerCallback(SIGTERM, &serverShutdown);
                      Sig.registerCallback(SIGCHLD, &childExited);
                      Sig.ignore(SIGPIPE);
                      Sig.enable();
                    },
                    [] {
                      SignalHandling& Sig = SignalHandling::get();
                      Sig.disable();
                      Sig.unignore(SIGPIPE);
                      Sig.clearCallback(SIGCHLD);
                      Sig.clearCallback(SIGTERM);
                      Sig.clearCallback(SIGINT);
                      Sig.deleteObject("Server");
                    }};

  LOG(info) << "Starting Monomux Server";
  ScopeGuard Server{[&S] { S.loop(); }, [&S] { S.shutdown(); }};
  LOG(info) << "Monomux Server stopped";
  return EXIT_Success;
}

} // namespace monomux::server
