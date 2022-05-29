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

#include "monomux/adt/Lazy.hpp"
#include "monomux/adt/ScopeGuard.hpp"
#include "monomux/client/Client.hpp"
#include "monomux/client/ControlClient.hpp"
#include "monomux/client/Terminal.hpp"
#include "monomux/system/Environment.hpp"
#include "monomux/system/Signal.hpp"
#include "monomux/system/Time.hpp"
#include "monomux/unreachable.hpp"

#include "monomux/system/Process.hpp"

#include "ExitCode.hpp"
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

bool makeWholeWithData(Client& Client, std::string* FailureReason)
{
  static constexpr std::size_t MaxHandshakeTries = 16;
  unsigned short HandshakeCounter = 0;
  while (!Client.handshake(FailureReason))
  {
    ++HandshakeCounter;
    if (HandshakeCounter == MaxHandshakeTries)
    {
      if (FailureReason)
        FailureReason->insert(
          0, "Failed to establish full connection after enough retries. ");
      return false;
    }

    LOG(warn) << "Establishing full connection failed:\n\t" << FailureReason;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return true;
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

void emplaceDefaultProgram(Options& Opts);
SessionSelectionResult selectSession(const std::vector<SessionData>& Sessions,
                                     const std::string& ToCreateSessionName);
SessionSelectionResult selectSession(const std::string& ClientID,
                                     const std::string& DefaultProgram,
                                     const std::vector<SessionData>& Sessions,
                                     const std::string& ToCreateSessionName,
                                     bool ListSessions,
                                     bool Interactive);
ExitCode mainForControlClient(Options& Opts);
ExitCode handleSessionCreateOrAttach(Options& Opts);
int handleClientExitStatus(const Client& Client);

void windowSizeChange(SignalHandling::Signal SigNum,
                      ::siginfo_t* Info,
                      const SignalHandling* Handling);
void coreDumped(SignalHandling::Signal SigNum,
                ::siginfo_t* Info,
                const SignalHandling* Handling);

// NOLINTNEXTLINE(cert-err58-cpp)
auto OriginalLogLevel =
  makeLazy([]() -> log::Severity { return log::Logger::get().getLimit(); });

constexpr char TerminalObjName[] = "Terminal";

} // namespace

int main(Options& Opts)
{
  // For the convenience of auto-starting a server if none exists, the creation
  // of the Client itself is placed into the global entry point.
  if (!Opts.Connection.has_value())
  {
    LOG(fatal) << "Attempted to start Client without an active connection!";
    return EXIT_SystemError;
  }
  Client& Client = *Opts.Connection;

  if (Opts.isControlMode())
    return mainForControlClient(Opts);

  if (ExitCode Err = handleSessionCreateOrAttach(Opts); Err != EXIT_Success)
    return Err;
  if (!Client.attached())
    // If there was an error connecting, the previous conditional exited with
    // the error.
    return EXIT_Success;

  // ----------------------------- Be a real client ----------------------------
  Terminal Term{fd::fileno(stdin), fd::fileno(stdout)};

  {
    // Ask the remote program to redraw by generating the "window size changed"
    // signal explicitly.
    Client.sendSignal(SIGWINCH);

    // Send the initial window size to the server so the attached session prompt
    // is appropriately (re)drawn to the right size, if the size is different.
    Terminal::Size S = Term.getSize();
    LOG(data) << "Terminal size: rows=" << S.Rows << ", columns=" << S.Columns;

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
                        Sig.registerObject(SignalHandling::ModuleObjName,
                                           "Client");
                        Sig.registerObject(TerminalObjName, &Term);
                        Sig.registerCallback(SIGWINCH, &windowSizeChange);
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
                        Sig.defaultCallback(SIGWINCH);
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

    Client.loop();
  }

  LOG(trace) << "Client stopped...";
  return handleClientExitStatus(Client);
}

namespace
{

void emplaceDefaultProgram(Options& Opts)
{
  const bool HasReceivedProgramToStart =
    Opts.Program && !Opts.Program->Program.empty();
  std::string DefaultProgram =
    HasReceivedProgramToStart ? Opts.Program->Program : defaultShell();
  if (DefaultProgram.empty())
    LOG(warn) << "Failed to figure out what shell is being used, and no good "
                 "defaults are available.\nPlease set the SHELL environment "
                 "variable.";
  if (!HasReceivedProgramToStart)
  {
    if (!Opts.Program)
      Opts.Program.emplace(Process::SpawnOptions{});
    Opts.Program->Program = DefaultProgram;
  }
}


SessionSelectionResult selectSession(const std::vector<SessionData>& Sessions,
                                     const std::string& ToCreateSessionName)
{
  if (Sessions.empty())
  {
    LOG(debug) << "List of sessions on server is empty, requesting default...";
    return {ToCreateSessionName, SessionSelectionResult::Create};
  }

  if (ToCreateSessionName.empty() && Sessions.size() == 1)
  {
    LOG(debug) << "No session '--name' specified, attaching to the singular "
                  "existing session...";
    return {Sessions.front().Name, SessionSelectionResult::Attach};
  }

  if (!ToCreateSessionName.empty())
  {
    LOG(debug) << "Session \"" << ToCreateSessionName
               << "\" requested, checking...";
    for (const SessionData& S : Sessions)
      if (S.Name == ToCreateSessionName)
      {
        LOG(debug) << "\tFound requested session, preparing for attach...";
        return {S.Name, SessionSelectionResult::Attach};
      }

    LOG(debug) << "\tRequested session not found, requesting spawn...";
    return {ToCreateSessionName, SessionSelectionResult::Create};
  }

  return {"", SessionSelectionResult::None};
}

SessionSelectionResult selectSession(const std::string& ClientID,
                                     const std::string& DefaultProgram,
                                     const std::vector<SessionData>& Sessions,
                                     const std::string& ToCreateSessionName,
                                     bool UserWantsOnlyListSessions,
                                     bool UserWantsInteractive)
{
  if (!(UserWantsOnlyListSessions || UserWantsInteractive))
  {
    // Unless we know already that interactivity is needed, try the default
    // logic...
    SessionSelectionResult R = selectSession(Sessions, ToCreateSessionName);
    if (R.Mode != SessionSelectionResult::None)
      // If decision making was successful, pass it on.
      return R;
    // If the non-interactive logic did not work out, fall back to
    // interactivity.
  }

  const std::size_t NewSessionChoice = Sessions.size() + 1;
  const std::size_t QuitChoice = NewSessionChoice + 1;
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
    if (UserWantsOnlyListSessions)
    {
      std::cout << std::endl;
      return {"", SessionSelectionResult::None};
    }

    // ---------------------- Show the interactive menu -----------------------
    std::cout << "    " << NewSessionChoice << ". Create a new ";
    if (!ToCreateSessionName.empty())
      std::cout << '\'' << ToCreateSessionName << "' ";
    std::cout << "session (" << DefaultProgram << ")\n";

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
  {
    if (!ToCreateSessionName.empty())
      return {ToCreateSessionName, SessionSelectionResult::Create};

    std::cout << "\nSession name (leave blank for default): ";
    std::string ToCreateSessionName2;
    std::getline(std::cin, ToCreateSessionName2);
    std::cout << std::endl;
    return {ToCreateSessionName2, SessionSelectionResult::Create};
  }
  if (UserChoice == QuitChoice)
    return {"", SessionSelectionResult::None};
  return {Sessions.at(UserChoice - 1).Name, SessionSelectionResult::Attach};
}

/// Handles operations through a \p ControlClient -only connection.
ExitCode mainForControlClient(Options& Opts)
{
  if (Opts.StatisticsRequest)
  {
    ControlClient CC{*Opts.Connection};
    try
    {
      std::string Stats = CC.requestStatistics();
      std::cout << Stats << std::endl;
      return EXIT_Success;
    }
    catch (const std::runtime_error& Err)
    {
      std::cerr << Err.what() << std::endl;
      return EXIT_SystemError;
    }
  }

  if (!Opts.SessionData)
    Opts.SessionData = MonomuxSession::loadFromEnv();
  if (!Opts.SessionData)
  {
    std::cerr << "In-session options require the client to be executed "
                 "within a session!"
              << std::endl;
    return EXIT_InvocationError;
  }

  ControlClient CC{*Opts.Connection, std::move(Opts.SessionData->SessionName)};
  if (!Opts.Connection->attached())
  {
    LOG(fatal) << "Failed to attach to session \"" << CC.sessionName() << "\"!";
    return EXIT_SystemError;
  }

  if (Opts.DetachRequestLatest)
    CC.requestDetachLatestClient();
  else if (Opts.DetachRequestAll)
    CC.requestDetachAllClients();

  return EXIT_Success;
}


ExitCode handleSessionCreateOrAttach(Options& Opts)
{
  Client& Client = *Opts.Connection;

  std::optional<std::vector<SessionData>> Sessions =
    Client.requestSessionList();
  if (!Sessions.has_value())
  {
    LOG(fatal) << "Receiving the list of sessions from the server failed!";
    return EXIT_SystemError;
  }

  emplaceDefaultProgram(Opts);
  SessionSelectionResult SessionAction =
    selectSession(Client.getControlSocket().identifier(),
                  Opts.Program->Program,
                  *Sessions,
                  Opts.SessionName ? *Opts.SessionName : "",
                  Opts.OnlyListSessions,
                  Opts.InteractiveSessionMenu);
  if (SessionAction.Mode == SessionSelectionResult::None)
    return EXIT_Success;
  if (SessionAction.Mode == SessionSelectionResult::Create)
  {
    assert(Opts.Program && !Opts.Program->Program.empty() &&
           "When creating a new session, no program to start was set");
    // TODO: Clean up the environment variables of the to-be-spawned process,
    // e.g. do not inherit the TERM of the server, but rather the TERM of the
    // client.

    std::optional<std::string> Response = Client.requestMakeSession(
      SessionAction.SessionName, std::move(*Opts.Program));
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
    {
      std::string DataFailure;
      if (!makeWholeWithData(Client, &DataFailure))
      {
        LOG(fatal) << DataFailure;
        return EXIT_SystemError;
      }
    }

    LOG(debug) << "Attaching to \"" << SessionAction.SessionName << "\"...";
    bool Attached = Client.requestAttach(std::move(SessionAction.SessionName));
    if (!Attached)
    {
      std::cerr << "ERROR: Server reported failure when attaching."
                << std::endl;
      return EXIT_SystemError;
    }
  }

  return EXIT_Success;
}

int handleClientExitStatus(const Client& Client)
{
  std::cout << std::endl;

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
      std::cout << ']' << std::endl;
      return EXIT_Success;
    case Client::SessionExit:
      std::cout << "[exited";
      if (Client.exitCode())
        std::cout << " (with return code " << Client.exitCode() << ')';
      if (const SessionData* S = Client.attachedSession())
        std::cout << " (from session '" << S->Name << "')";
      std::cout << ']' << std::endl;
      return Client.exitCode();
    case Client::ServerExit:
      std::cout << "[server exited]" << std::endl;
      return EXIT_Success;
    case Client::ServerKicked:
      std::cout << "[booted from server";
      if (!Client.exitMessage().empty())
        std::cout << ": " << Client.exitMessage();
      std::cout << ']' << std::endl;
  }
  return EXIT_Success;
}

/// Handler for \p SIGWINCH (window size change) events produces by the terminal
/// the program is running in.
void windowSizeChange(SignalHandling::Signal /* SigNum */,
                      ::siginfo_t* /* Info */,
                      const SignalHandling* Handling)
{
  const volatile auto* Term =
    std::any_cast<Terminal*>(Handling->getObject(TerminalObjName));
  if (!Term)
    return;
  (*Term)->notifySizeChanged();
}

/// Custom handler for \p SIGABRT. This is even more custom than the handler in
/// the global \p main() as it deals with resetting the terminal to sensible
/// defaults first.
void coreDumped(SignalHandling::Signal /* SigNum */,
                ::siginfo_t* /* Info */,
                const SignalHandling* Handling)
{
  // Reset the loglevel so all messages that might appear appear as needed.
  log::Logger::get().setLimit(OriginalLogLevel.get());

  // Reset the terminal so we don't get weird output if the client crashed in
  // the middle of a formatting sequence.
  const volatile auto* Term =
    std::any_cast<Terminal*>(Handling->getObject(TerminalObjName));
  if (Term)
  {
    (*Term)->disengage();
    (*Term)->releaseClient();
  }
}

} // namespace

} // namespace monomux::client

#undef LOG
