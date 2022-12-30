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
#include <chrono>
#include <iostream>
#include <thread>

#include "monomux/FrontendExitCode.hpp"
#include "monomux/adt/Lazy.hpp"
#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/client/App.hpp"
#include "monomux/client/Client.hpp"
#include "monomux/client/ControlClient.hpp"
#include "monomux/client/SessionManagement.hpp"
#include "monomux/client/Terminal.hpp" // FIXME: Unguarded dependence on ::unix.
#include "monomux/system/Environment.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/SignalHandling.hpp"
#include "monomux/unreachable.hpp"

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/UnixTerminal.hpp"
#include "monomux/system/fd.hpp"
#endif /* MONOMUX_PLATFORM_UNIX */

#include "monomux/client/Main.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("client/Main")

namespace monomux::client
{

Options::Options()
  : ClientMode(false), OnlyListSessions(false), InteractiveSessionMenu(false),
    DetachRequestLatest(false), DetachRequestAll(false),
    StatisticsRequest(false)
{}

std::vector<std::string> Options::toArgv() const
{
  std::vector<std::string> Ret;

  if (SessionName.has_value())
  {
    Ret.emplace_back("--name");
    Ret.emplace_back(*SessionName);
  }
  if (SocketPath.has_value())
  {
    Ret.emplace_back("--socket");
    Ret.emplace_back(*SocketPath);
  }

  if (InteractiveSessionMenu)
    Ret.emplace_back("--interactive");
  else if (OnlyListSessions)
    Ret.emplace_back("--list");

  if (DetachRequestLatest)
    Ret.emplace_back("--detach");
  if (DetachRequestAll)
    Ret.emplace_back("--detach-all");
  if (StatisticsRequest)
    Ret.emplace_back("--statistics");

  if (Program)
  {
    for (const auto& Env : Program->Environment)
    {
      if (Env.second.has_value())
      {
        Ret.emplace_back("--env");
        Ret.emplace_back(Env.first).append("=").append(*Env.second);
      }
      else
      {
        Ret.emplace_back("--unset");
        Ret.emplace_back(Env.first);
      }
    }

    Ret.emplace_back("--");
    Ret.emplace_back(Program->Program);

    for (const std::string& Arg : Program->Arguments)
      Ret.emplace_back(Arg);
  }

  return Ret;
}

bool Options::isControlMode() const noexcept
{
  return DetachRequestLatest || DetachRequestAll || StatisticsRequest;
}

system::MonomuxSession getEnvironmentalSession(const Options& Opts)
{
  using namespace monomux::system;
  system::MonomuxSession Session{};

  if (Opts.isControlMode() && !Opts.SocketPath)
  {
    // Load a session from the current process's environment, to have a socket
    // for the controller client ready, if needed.
    std::optional<MonomuxSession> Session = MonomuxSession::loadFromEnv();
    if (Session)
      return std::move(*Session);
  }

  Platform::SocketPath ElaborateSocketPath =
    Opts.SocketPath.has_value()
      ? Platform::SocketPath::absolutise(*Opts.SocketPath)
      : Platform::SocketPath::defaultSocketPath();
  Session.Socket = std::move(ElaborateSocketPath);
  return Session;
}

std::optional<Client>
connect(Options& Opts, bool Block, std::string* FailureReason)
{
  static constexpr std::size_t MaxConnectTries = 4;
  unsigned short ConnectCounter = 0;
  std::optional<Client> C;
  while (!C)
  {
    ++ConnectCounter;
    MONOMUX_TRACE_LOG(
      if (Block) {
        LOG(debug) << '#' << ConnectCounter << ' ' << "Attempt connecting to '"
                   << *Opts.SocketPath << "'...";
      } else {
        LOG(debug) << "Attempt connecting to '" << *Opts.SocketPath << "'...";
      });
    if (ConnectCounter == MaxConnectTries)
    {
      if (FailureReason)
        FailureReason->insert(
          0, "Failed to establish connection after enough retries. ");
      return std::nullopt;
    }

    try
    {
      C = Client::create(*Opts.SocketPath, FailureReason);
      if (!Block || C.has_value())
        return C;
    }
    catch (...)
    {
      if (!Block)
        throw;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return C;
}

bool makeWholeWithData(Client& Client, std::string* FailureReason)
{
  static constexpr std::size_t MaxHandshakeTries = 4;
  unsigned short HandshakeCounter = 0;
  while (!Client.handshake(FailureReason))
  {
    ++HandshakeCounter;
    MONOMUX_TRACE_LOG(LOG(debug) << '#' << HandshakeCounter << ' '
                                 << "Attempt connecting data...");
    if (HandshakeCounter == MaxHandshakeTries)
    {
      if (FailureReason)
        FailureReason->insert(
          0, "Failed to establish full connection after enough retries. ");
      return false;
    }

    LOG(warn) << "Establishing full connection failed:\n\t"
              << (FailureReason ? *FailureReason : "No reason given.");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return true;
}

namespace
{

FrontendExitCode mainForControlClient(Options& Opts);
int handleClientExitStatus(const Client& Client);

MONOMUX_SIGNAL_HANDLER(coreDumped);

// NOLINTNEXTLINE(cert-err58-cpp)
auto OriginalLogLevel =
  makeLazy([]() -> log::Severity { return log::Logger::get().getLimit(); });

constexpr char TerminalObjName[] = "Terminal";

} // namespace

int main(Options& Opts)
{
  using namespace monomux::system;

  // For the convenience of auto-starting a server if none exists, the creation
  // of the Client itself is placed into the global entry point.
  if (!Opts.Connection.has_value())
  {
    LOG(fatal) << "Attempted to start Client without an active connection!";
    return static_cast<int>(FrontendExitCode::SystemError);
  }
  Client& Client = *Opts.Connection;

  if (Opts.isControlMode())
    return static_cast<int>(mainForControlClient(Opts));

  if (FrontendExitCode Err = sessionCreateOrAttach(Opts);
      Err != FrontendExitCode::Success)
    return static_cast<int>(Err);
  if (!Client.attached())
    // If there was an error connecting, the previous conditional exited with
    // the error.
    return static_cast<int>(FrontendExitCode::Success);

#ifdef MONOMUX_PLATFORM_UNIX
  std::shared_ptr<unix::Terminal> UnixTerminal =
    unix::Terminal::create(unix::fd::fileno(stdin), unix::fd::fileno(stdout));
  {
    // Ask the remote program to redraw by generating the "window size changed"
    // signal explicitly.
    Client.sendSignal(SIGWINCH);

    // Send the initial window size to the server so the attached session prompt
    // is appropriately (re)drawn to the right size, if the size is different.
    unix::Terminal::Size S = UnixTerminal->getSize();
    LOG(data) << "Terminal size: rows=" << S.Rows << ", columns=" << S.Columns;

    // This is a little bit of a hack, but we observed that certain elaborate
    // prompts, such as multiline ZSH Powerline do not really redraw when the
    // size remains the same.
    Client.notifyWindowSize(S.Rows - 1, S.Columns - 1);

    Client.notifyWindowSize(S.Rows, S.Columns);
  }
#endif /* MONOMUX_PLATFORM_UNIX */

  // FIXME: Unguarded dependence on ::unix.
  Terminal Term{system::unix::fd::fileno(stdin),
                system::unix::fd::fileno(stdout)};

  {
    ScopeGuard TerminalSetup{[&Term, &Client] { Term.setupClient(Client); },
                             [&Term] { Term.releaseClient(); }};
    ScopeGuard Signal{[&Term] {
                        SignalHandling& Sig = SignalHandling::get();
                        LOG(error) << "Register Client module";
                        Sig.registerObject(SignalHandling::ModuleObjName,
                                           "Client");
                        Sig.registerObject(TerminalObjName, &Term);
                        Sig.enable();

                        // Override the SIGABRT handler with a custom one that
                        // resets the terminal during a crash.
                        Sig.registerCallback(SIGILL, &coreDumped);
                        Sig.registerCallback(SIGABRT, &coreDumped);
                        Sig.registerCallback(SIGSEGV, &coreDumped);
                        Sig.registerCallback(SIGSYS, &coreDumped);
                        Sig.registerCallback(SIGSTKFLT, &coreDumped);
                      },
                      [] {
                        SignalHandling& Sig = SignalHandling::get();
                        Sig.deleteObject(TerminalObjName);

                        Sig.clearOneCallback(SIGILL);
                        Sig.clearOneCallback(SIGABRT);
                        Sig.clearOneCallback(SIGSEGV);
                        Sig.clearOneCallback(SIGSYS);
                        Sig.clearOneCallback(SIGSTKFLT);
                      }};

    LOG(trace) << "Starting client...";

    // Turn off all logging from this point now on, because the attached client
    // randomly printing log to stdout would garble the terminal printouts.
    OriginalLogLevel.get(); // Load it in.
    ScopeGuard Loglevel{
      [] { log::Logger::get().setLimit(log::None); },
      [] { log::Logger::get().setLimit(OriginalLogLevel.get()); }};

    ScopeGuard TermIO{[&Term] { Term.engage(); },
                      [&Term] { Term.disengage(); }};

#ifdef MONOMUX_PLATFORM_UNIX
    ScopeGuard UnixTerminalSetup{
      [&Sig = SignalHandling::get(), &Term = UnixTerminal] {
        Term->setupListenForSizeChangeSignal(Sig);
        Sig.enable();

        Term->setRawMode();
      },
      [&Sig = SignalHandling::get(), &Term = UnixTerminal] {
        Term->setOriginalMode();

        Term->teardownListenForSizeChangeSignal(Sig);
      }};
#endif /* MONOMUX_PLATFORM_UNIX */

    Client.loop();
  }

  LOG(trace) << "Client stopped...";
  return handleClientExitStatus(Client);
}

namespace
{

/// Handles operations through a \p ControlClient -only connection.
FrontendExitCode mainForControlClient(Options& Opts)
{
  if (Opts.StatisticsRequest)
  {
    ControlClient CC{*Opts.Connection};
    try
    {
      std::string Stats = CC.requestStatistics();
      std::cout << Stats << std::endl;
      return FrontendExitCode::Success;
    }
    catch (const std::runtime_error& Err)
    {
      std::cerr << Err.what() << std::endl;
      return FrontendExitCode::SystemError;
    }
  }

  if (!Opts.SessionData)
    Opts.SessionData = system::MonomuxSession::loadFromEnv();
  if (!Opts.SessionData)
  {
    std::cerr << "In-session options require the client to be executed "
                 "within a session!"
              << std::endl;
    return FrontendExitCode::InvocationError;
  }

  ControlClient CC{*Opts.Connection, std::move(Opts.SessionData->SessionName)};
  if (!Opts.Connection->attached())
  {
    LOG(fatal) << "Failed to attach to session \"" << CC.sessionName() << "\"!";
    return FrontendExitCode::SystemError;
  }

  if (Opts.DetachRequestLatest)
    CC.requestDetachLatestClient();
  else if (Opts.DetachRequestAll)
    CC.requestDetachAllClients();

  return FrontendExitCode::Success;
}

int handleClientExitStatus(const Client& Client)
{
  std::cout << std::endl;

  Client::Exit E = Client.getExitData();
  switch (E.Reason)
  {
    case Client::Exit::None:
      std::cout << "[unknown reason]" << std::endl;
      return static_cast<int>(FrontendExitCode::SystemError);
    case Client::Exit::Failed:
      std::cout << "[lost server]" << std::endl;
      return static_cast<int>(FrontendExitCode::SystemError);
    case Client::Exit::Terminated:
      std::cout << "[terminated]" << std::endl;
      return static_cast<int>(FrontendExitCode::Success);
    case Client::Exit::Hangup:
      std::cout << "[lost tty]" << std::endl;
      return static_cast<int>(FrontendExitCode::Failure);
    case Client::Exit::Detached:
      std::cout << "[detached";
      if (const SessionData* S = Client.attachedSession())
        std::cout << " (from session '" << S->Name << "')";
      std::cout << ']' << std::endl;
      return static_cast<int>(FrontendExitCode::Success);
    case Client::Exit::SessionExit:
      std::cout << "[exited";
      if (E.SessionExitCode)
        std::cout << " (with return code " << E.SessionExitCode << ')';
      if (const SessionData* S = Client.attachedSession())
        std::cout << " (from session '" << S->Name << "')";
      std::cout << ']' << std::endl;
      return E.SessionExitCode;
    case Client::Exit::ServerExit:
      std::cout << "[server exited]" << std::endl;
      return static_cast<int>(FrontendExitCode::Success);
    case Client::Exit::ServerKicked:
      std::cout << "[booted from server";
      if (!E.Message.empty())
        std::cout << ": " << E.Message;
      std::cout << ']' << std::endl;
  }
  return static_cast<int>(FrontendExitCode::Success);
}

/// Custom handler for \p SIGABRT. This is even more custom than the handler in
/// the global \p main() as it deals with resetting the terminal to sensible
/// defaults first.
MONOMUX_SIGNAL_HANDLER(coreDumped)
{
  (void)Sig;
  (void)PlatformInfo;

  // Reset the loglevel so all messages that might appear appear as needed.
  log::Logger::get().setLimit(OriginalLogLevel.get());

  // Reset the terminal so we don't get weird output if the client crashed in
  // the middle of a formatting sequence.
  // const volatile auto* Term =
  //   SignalHandling->getObjectAs<Terminal*>(TerminalObjName);
  // if (Term)
  // {
  //   (*Term)->disengage();
  //   (*Term)->releaseClient();
  // }
}

} // namespace

} // namespace monomux::client

#undef LOG
