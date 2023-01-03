/* SPDX-License-Identifier: GPL-3.0-only */
#include <memory>
#include <thread>

#include "monomux/CheckedErrno.hpp"
#include "monomux/Config.h"
#include "monomux/FrontendExitCode.hpp"
#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/server/Server.hpp"
#include "monomux/system/Environment.hpp"
#include "monomux/system/Platform.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/SignalHandling.hpp"
#include "monomux/unreachable.hpp"

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/UnixDomainSocket.hpp"
#endif /* MONOMUX_PLATFORM_UNIX */

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

  system::Process::SpawnOptions SO;
  SO.Program = ArgV0;
  SO.Arguments = Opts.toArgv();

  system::Process::exec(SO);
  unreachable("[[noreturn]]");
}

namespace
{

constexpr char ServerObjName[] = "Server";

MONOMUX_SIGNAL_HANDLER(serverShutdown);
MONOMUX_SIGNAL_HANDLER(childExited);
MONOMUX_SIGNAL_HANDLER(coreDumped);

} // namespace

FrontendExitCode main(Options& Opts)
{
  using namespace monomux::system;

  std::unique_ptr<system::Socket> ServerSock;
  try
  {
#if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_ID_Unix
    ServerSock = std::make_unique<unix::DomainSocket>(
      unix::DomainSocket::create(*Opts.SocketPath));
#else  /* Unhandled platform */
    LOG(fatal)
      << MONOMUX_FEED_PLATFORM_NOT_SUPPORTED_MESSAGE
      << "socket-based communication, but this is required to accept clients."
      << '\n';
    return FrontendExitCode::SystemError;
#endif /* MONOMUX_PLATFORM_ID */
  }
  catch (const std::system_error& SE)
  {
    LOG(fatal) << "Creating the socket '" << *Opts.SocketPath << "' failed:\n\t"
               << SE.what();
    if (SE.code() == std::errc::address_in_use)
      LOG(info) << "If you are sure another server is not running, delete the "
                   "file and restart the server.";
    return FrontendExitCode::SystemError;
  }

  Server S = Server(std::move(ServerSock));
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
    CheckedErrnoThrow(
      [] { return ::daemon(0, 0); }, "Backgrounding ourselves failed", -1);

  ScopeGuard Server{[&S] { S.loop(); }, [&S] { S.shutdown(); }};
  LOG(info) << "Monomux Server stopped";
  return FrontendExitCode::Success;
}

namespace
{

/// Handler for request to terinate the server.
MONOMUX_SIGNAL_HANDLER(serverShutdown)
{
  (void)Sig;
  (void)PlatformInfo;

  const volatile auto* Srv =
    SignalHandling->getObjectAs<Server*>(ServerObjName);
  if (!Srv)
    return;
  (*Srv)->interrupt();
}

/// Handler for \p SIGCHLD when a process spawned by the server quits.
#ifdef MONOMUX_PLATFORM_UNIX
MONOMUX_SIGNAL_HANDLER(childExited)
{
  (void)Sig;

  system::Process::Raw CPID = PlatformInfo->si_pid;
  const volatile auto* Srv =
    SignalHandling->getObjectAs<Server*>(ServerObjName);
  if (!Srv)
    return;
  (*Srv)->registerDeadChild(CPID);
}
#endif /* MONOMUX_PLATFORM_UNIX */

/// Custom handler for \p SIGABRT. This is even more custom than the handler in
/// the global \p main() as it deals with killing the server first.
MONOMUX_SIGNAL_HANDLER(coreDumped)
{
  serverShutdown(Sig, SignalHandling, PlatformInfo);
}

} // namespace

} // namespace monomux::server

#undef LOG
