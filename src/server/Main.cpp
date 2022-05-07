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
#include "monomux/server/Server.hpp"
#include "monomux/system/Environment.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/Signal.hpp"
#include "monomux/unreachable.hpp"

#include "ExitCode.hpp"
#include "monomux/server/Main.hpp"

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

namespace
{

constexpr char ServerObjName[] = "Server";

void serverShutdown(SignalHandling::Signal SigNum,
                    ::siginfo_t* Info,
                    const SignalHandling* Handling);
void childExited(SignalHandling::Signal SigNum,
                 ::siginfo_t* Info,
                 const SignalHandling* Handling);
void coreDumped(SignalHandling::Signal SigNum,
                ::siginfo_t* Info,
                const SignalHandling* Handling);

} // namespace

int main(Options& Opts)
{
  std::optional<Socket> ServerSock;
  try
  {
    ServerSock.emplace(Socket::create(*Opts.SocketPath));
  }
  catch (const std::system_error& SE)
  {
    LOG(fatal) << "Creating the socket '" << *Opts.SocketPath << "' failed:\n\t"
               << SE.what();
    if (SE.code() == std::errc::address_in_use)
      LOG(info) << "If you are sure another server is not running, delete the "
                   "file and restart the server.";
    return EXIT_SystemError;
  }

  Server S = Server(std::move(*ServerSock));
  S.setExitIfNoMoreSessions(Opts.ExitOnLastSessionTerminate);
  ScopeGuard Signal{[&S] {
                      SignalHandling& Sig = SignalHandling::get();
                      Sig.registerObject(SignalHandling::ModuleObjName,
                                         "Server");
                      Sig.registerObject(ServerObjName, &S);
                      Sig.registerCallback(SIGHUP, &serverShutdown);
                      Sig.registerCallback(SIGINT, &serverShutdown);
                      Sig.registerCallback(SIGTERM, &serverShutdown);
                      Sig.registerCallback(SIGCHLD, &childExited);
                      Sig.ignore(SIGPIPE);
                      Sig.enable();

                      // Override the SIGABRT handler with a custom one that
                      // kills the server.
                      Sig.registerCallback(SIGILL, &coreDumped);
                      Sig.registerCallback(SIGABRT, &coreDumped);
                      Sig.registerCallback(SIGSEGV, &coreDumped);
                      Sig.registerCallback(SIGSYS, &coreDumped);
                      Sig.registerCallback(SIGSTKFLT, &coreDumped);
                    },
                    [] {
                      SignalHandling& Sig = SignalHandling::get();
                      Sig.unignore(SIGPIPE);
                      Sig.defaultCallback(SIGCHLD);
                      Sig.defaultCallback(SIGTERM);
                      Sig.defaultCallback(SIGINT);
                      Sig.defaultCallback(SIGHUP);
                      Sig.deleteObject(ServerObjName);

                      Sig.clearOneCallback(SIGILL);
                      Sig.clearOneCallback(SIGABRT);
                      Sig.clearOneCallback(SIGSEGV);
                      Sig.clearOneCallback(SIGSYS);
                      Sig.clearOneCallback(SIGSTKFLT);
                    }};

  LOG(info) << "Starting Monomux Server";
  if (Opts.Background)
    CheckedPOSIXThrow(
      [] { return ::daemon(0, 0); }, "Backgrounding ourselves failed", -1);

  ScopeGuard Server{[&S] { S.loop(); }, [&S] { S.shutdown(); }};
  LOG(info) << "Monomux Server stopped";
  return EXIT_Success;
}

namespace
{

/// Handler for request to terinate the server.
void serverShutdown(SignalHandling::Signal /* SigNum */,
                    ::siginfo_t* /* Info */,
                    const SignalHandling* Handling)
{
  const volatile auto* Srv =
    std::any_cast<Server*>(Handling->getObject(ServerObjName));
  if (!Srv)
    return;
  (*Srv)->interrupt();
}

/// Handler for \p SIGCHLD when a process spawned by the server quits.
void childExited(SignalHandling::Signal /* SigNum */,
                 ::siginfo_t* Info,
                 const SignalHandling* Handling)
{
  Process::raw_handle CPID = Info->si_pid;
  const volatile auto* Srv =
    std::any_cast<Server*>(Handling->getObject(ServerObjName));
  if (!Srv)
    return;
  (*Srv)->registerDeadChild(CPID);
}

/// Custom handler for \p SIGABRT. This is even more custom than the handler in
/// the global \p main() as it deals with killing the server first.
void coreDumped(SignalHandling::Signal SigNum,
                ::siginfo_t* Info,
                const SignalHandling* Handling)
{
  serverShutdown(SigNum, Info, Handling);
}

} // namespace

} // namespace monomux::server
