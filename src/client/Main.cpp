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

#include "Client.hpp"
#include "ControlClient.hpp"
#include "Main.hpp"
#include "Terminal.hpp"

#include "ExitCode.hpp"

#include "system/Environment.hpp"
#include "system/Signal.hpp"

#include "monomux/Log.hpp"
#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/system/Time.hpp"

#define LOG(SEVERITY) monomux::log::SEVERITY("client/Main")

namespace monomux::client
{

Options::Options()
  : ClientMode(false), ForceSessionSelectMenu(false),
    DetachRequestLatest(false), DetachRequestAll(false)
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
  if (ForceSessionSelectMenu)
    Ret.emplace_back("--list");
  if (DetachRequestLatest)
    Ret.emplace_back("--detach");
  if (DetachRequestAll)
    Ret.emplace_back("--detach-all");

  if (Program.has_value())
  {
    Ret.emplace_back(*Program);

    for (const std::string& Arg : ProgramArgs)
      Ret.emplace_back(Arg);
  }

  return Ret;
}

bool Options::isControlMode() const noexcept
{
  return DetachRequestLatest || DetachRequestAll;
}

std::optional<Client>
connect(Options& Opts, bool Block, std::string* FailureReason)
{
  auto C = Client::create(*Opts.SocketPath, FailureReason);
  if (!Block)
    return C;

  static constexpr std::size_t MaxConnectTries = 16;
  unsigned short ConnectCounter = 0;
  while (!C)
  {
    ++ConnectCounter;
    if (ConnectCounter == MaxConnectTries)
    {
      if (FailureReason)
        *FailureReason = "Connection failed after enough retries.";
      return std::nullopt;
    }

    C = Client::create(*Opts.SocketPath, FailureReason);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return C;
}

/// Handler for \p SIGWINCH (window size change) events produces by the terminal
/// the program is running in.
static void windowSizeChange(SignalHandling::Signal /* SigNum */,
                             ::siginfo_t* /* Info */,
                             const SignalHandling* Handling)
{
  const volatile auto* Term =
    std::any_cast<Terminal*>(Handling->getObject("Terminal"));
  if (!Term)
    return;
  (*Term)->notifySizeChanged();
}

namespace
{

struct SessionSelectionResult
{
  std::string SessionName;

  enum SessionMode
  {
    None,
    /// The client should instruct the server to create the selected session.
    Create,
    /// The user selected only attaching to an already existing session.
    Attach
  };
  SessionMode Mode;
};

} // namespace

static SessionSelectionResult
selectSession(const std::string& ClientID,
              const std::string& DefaultProgram,
              const std::vector<SessionData>& Sessions,
              const std::string& DefaultSessionName,
              bool AlwaysShowMenu)
{
  if (!AlwaysShowMenu)
  {
    if (Sessions.empty())
    {
      LOG(debug)
        << "List of sessions on server is empty, requesting default...";
      return {DefaultSessionName, SessionSelectionResult::Create};
    }

    if (DefaultSessionName.empty() && Sessions.size() == 1)
    {
      LOG(debug) << "No session '--name' specified, attaching to the single "
                    "existing session...";
      return {Sessions.front().Name, SessionSelectionResult::Attach};
    }

    if (!DefaultSessionName.empty())
    {
      LOG(debug) << "Session \"" << DefaultSessionName
                 << "\" requested, checking...";
      for (const SessionData& S : Sessions)
        if (S.Name == DefaultSessionName)
        {
          LOG(debug) << "\tFound requested session, preparing for attach...";
          return {S.Name, SessionSelectionResult::Attach};
        }

      LOG(debug) << "\tRequested session not found, requesting spawn...";
      return {DefaultSessionName, SessionSelectionResult::Create};
    }

    assert(DefaultSessionName.empty() && Sessions.size() > 1);
  }

  std::size_t NewSessionChoice = Sessions.size() + 1;
  std::size_t QuitChoice = NewSessionChoice + 1;
  std::size_t UserChoice = 0;

  // Mimicking the layout of tmux/byobu menu.
  while (true)
  {
    std::cout << "\nMonomux sessions on '" << ClientID << "'...\n\n";
    for (std::size_t I = 0; I < Sessions.size(); ++I)
    {
      const SessionData& SD = Sessions.at(I);
      std::cout << "    " << (I + 1) << ". " << SD.Name << " (created "
                << formatTime(SD.Created) << ")\n";
    }
    std::cout << "    " << NewSessionChoice << ". Create a new session ("
              << DefaultProgram << ")\n";
    std::cout << "    " << QuitChoice << ". Quit\n";
    std::cout << "\nChoose 1-" << QuitChoice << ": ";

    std::cin >> UserChoice;
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (UserChoice == 0 || UserChoice > QuitChoice)
      std::cerr << "\nERROR: Invalid input" << std::endl;
    else
      break;
  }

  if (UserChoice == NewSessionChoice)
    return {DefaultSessionName, SessionSelectionResult::Create};
  if (UserChoice == QuitChoice)
    return {"", SessionSelectionResult::None};
  return {Sessions.at(UserChoice - 1).Name, SessionSelectionResult::Attach};
}

int main(Options& Opts)
{
  // For the convenience of auto-starting a server if none exists, the creation
  // of the Client itself is placed into the global entry point.
  if (!Opts.Connection.has_value())
  {
    LOG(fatal) << "Attempted to start Client without an active connection";
    return EXIT_SystemError;
  }
  Client& Client = *Opts.Connection;

  // ----------------------- Handle control-mode requests ----------------------
  if (Opts.isControlMode())
  {
    if (!Opts.SessionData)
      Opts.SessionData = MonomuxSession::loadFromEnv();
    if (!Opts.SessionData)
    {
      std::cerr << "In-session options require the client to be executed "
                   "within a session!"
                << std::endl;
      return EXIT_InvocationError;
    }

    ControlClient CC{Client, std::move(Opts.SessionData->SessionName)};
    if (!Client.attached())
    {
      LOG(fatal) << "Failed to attach to session \"" << CC.sessionName()
                 << "\"!";
      return EXIT_SystemError;
    }

    if (Opts.DetachRequestLatest)
      CC.requestDetachLatestClient();
    else if (Opts.DetachRequestAll)
      CC.requestDetachAllClients();

    return EXIT_Success;
  }

  // ----------------- Elevate the client to be fully connected ----------------
  {
    std::string FailureReason;
    static constexpr std::size_t MaxHandshakeTries = 16;
    unsigned short HandshakeCounter = 0;
    while (!Client.handshake(&FailureReason))
    {
      ++HandshakeCounter;
      if (HandshakeCounter == MaxHandshakeTries)
      {
        LOG(fatal)
          << "Failed to establish full connection after enough retries.";
        return EXIT_SystemError;
      }

      LOG(warn) << "Establishing full connection failed:\n\t" << FailureReason;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  // --------------------- Deduce what session to attach to --------------------
  {
    std::optional<std::vector<SessionData>> Sessions =
      Client.requestSessionList();
    if (!Sessions.has_value())
    {
      LOG(fatal) << "Receiving the list of sessions from the server failed!";
      return EXIT_SystemError;
    }

    std::string DefaultProgram = Opts.Program ? *Opts.Program : defaultShell();
    if (DefaultProgram.empty())
      LOG(warn) << "Failed to figure out what shell is being used, and no good "
                   "defaults are available.\nPlease set the SHELL environment "
                   "variable.";

    SessionSelectionResult SessionAction =
      selectSession(Client.getControlSocket().identifier(),
                    DefaultProgram,
                    *Sessions,
                    Opts.SessionName ? *Opts.SessionName : "",
                    Opts.ForceSessionSelectMenu);
    if (SessionAction.Mode == SessionSelectionResult::None)
      return EXIT_Success;
    if (SessionAction.Mode == SessionSelectionResult::Create)
    {
      Process::SpawnOptions Spawn;
      Spawn.Program = std::move(DefaultProgram);
      if (Spawn.Program.empty())
      {
        LOG(fatal)
          << "When creating a new session, no program to start was set.";
        return EXIT_InvocationError;
      }
      Spawn.Arguments = std::move(Opts.ProgramArgs);
      // TODO: Clean up the environment variables of the to-be-spawned process,
      // e.g. do not inherit the TERM of the server, but rather the TERM of the
      // client.

      std::optional<std::string> Response =
        Client.requestMakeSession(SessionAction.SessionName, std::move(Spawn));
      if (!Response.has_value() || Response->empty())
      {
        LOG(fatal) << "When creating a new session, the creation failed.";
        return EXIT_SystemError;
      }
      SessionAction.SessionName = *Response;

      SessionAction.Mode = SessionSelectionResult::Attach;
      // Intended "fallthrough".
    }
    if (SessionAction.Mode == SessionSelectionResult::Attach)
    {
      LOG(debug) << "Attaching to \"" << SessionAction.SessionName << "\"...";
      bool Attached =
        Client.requestAttach(std::move(SessionAction.SessionName));
      if (!Attached)
      {
        std::cerr << "ERROR: Server reported failure when attaching."
                  << std::endl;
        return EXIT_SystemError;
      }
    }
  }

  // ----------------------------- Be a real client ----------------------------
  Terminal Term{fd::fileno(stdin), fd::fileno(stdout)};

  {
    // Ask the remote program to redraw by generating the "window size changed"
    // signal explicitly.
    Client.sendSignal(SIGWINCH);

    // Send the initial window size to the server so the attached session prompt
    // is appropriately (re)drawn to the right size, if the size is different.
    Terminal::Size S = Term.getSize();
    LOG(data) << "Terminal size rows=" << S.Rows << ", columns=" << S.Columns;

    // This is a little bit of a hack, but we observed that certain elaborate
    // prompts, such as multiline ZSH Powerline do not really redraw when the
    // size remains the same.
    Client.notifyWindowSize(S.Rows - 1, S.Columns - 1);

    Client.notifyWindowSize(S.Rows, S.Columns);
  }

  {
    ScopeGuard TerminalSetup{[&Term, &Client] { Term.setupClient(Client); },
                             [&Term] { Term.releaseClient(); }};
    ScopeGuard Signal{[&Term] {
                        SignalHandling& Sig = SignalHandling::get();
                        Sig.registerObject("Terminal", &Term);
                        Sig.registerCallback(SIGWINCH, &windowSizeChange);
                        Sig.enable();
                      },
                      [] {
                        SignalHandling& Sig = SignalHandling::get();
                        Sig.clearCallback(SIGWINCH);
                        Sig.deleteObject("Terminal");
                        Sig.disable();
                      }};

    LOG(trace) << "Starting client...";

    // Turn off all logging from this point now on, because the attached client
    // randomly printing log to stdout would garble the terminal printouts.
    using namespace monomux::log;
    Severity LogLevel;
    ScopeGuard Loglevel{[&LogLevel] {
                          Logger& L = Logger::get();
                          LogLevel = L.getLimit();
                          L.setLimit(None);
                        },
                        [&LogLevel] { Logger::get().setLimit(LogLevel); }};

    ScopeGuard TermIO{[&Term] { Term.engage(); },
                      [&Term] { Term.disengage(); }};

    Client.loop();
  }

  LOG(trace) << "Client stopped...";

  switch (Client.exitReason())
  {
    case Client::None:
      std::cout << "[unknown reason]" << std::endl;
      return EXIT_SystemError;
    case Client::Failed:
      std::cout << "[lost server]" << std::endl;
      return EXIT_SystemError;
    case Client::Terminated:
      std::cout << "[terminated]" << std::endl;
      return EXIT_Success;
    case Client::Hangup:
      std::cout << "[lost tty]" << std::endl;
      return EXIT_Failure;
    case Client::Detached:
      std::cout << "[detached";
      if (const SessionData* S = Client.attachedSession())
        std::cout << " (from session '" << S->Name << "')";
      std::cout << "]" << std::endl;
      return EXIT_Success;
    case Client::SessionExit:
      std::cout << "[exited";
      if (const SessionData* S = Client.attachedSession())
        std::cout << " (from session '" << S->Name << "')";
      std::cout << "]" << std::endl;
      return EXIT_Success;
    case Client::ServerExit:
      std::cout << "[server exited]" << std::endl;
      return EXIT_Success;
  }

  return EXIT_Success;
}

} // namespace monomux::client
