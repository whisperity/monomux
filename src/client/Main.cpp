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
#include "Client.hpp"
#include "ControlClient.hpp"
#include "Terminal.hpp"

#include "ExitCode.hpp"
#include "server/Server.hpp" // FIXME: Do not depend on this.
#include "system/Environment.hpp"
#include "system/Time.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#include <termios.h>

namespace monomux
{
namespace client
{

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
      std::clog << "DEBUG: List of sessions on the server is empty, requesting "
                   "default one..."
                << std::endl;
      return {DefaultSessionName, SessionSelectionResult::Create};
    }

    if (DefaultSessionName.empty() && Sessions.size() == 1)
    {
      std::clog << "DEBUG: No session '--name' specified, attaching to only "
                   "existing session..."
                << std::endl;
      return {DefaultSessionName, SessionSelectionResult::Attach};
    }

    if (!DefaultSessionName.empty())
    {
      std::clog << "DEBUG: Session name '" << DefaultSessionName
                << "' specified, checking if it exists..." << std::endl;
      for (const SessionData& S : Sessions)
        if (S.Name == DefaultSessionName)
        {
          std::clog << "DEBUG: Session found, attaching!" << std::endl;
          return {S.Name, SessionSelectionResult::Attach};
        }

      std::clog << "DEBUG: Session not found, requesting creation..."
                << std::endl;
      return {DefaultSessionName, SessionSelectionResult::Create};
    }

    assert(DefaultSessionName.empty() && Sessions.size() > 1);
  }

  std::size_t NewSessionChoice = Sessions.size() + 1;
  std::size_t QuitChoice = NewSessionChoice + 1;
  std::size_t UserChoice = 0;

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
    std::cerr << "ERROR: Attempted to start client without active connection."
              << std::endl;
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
      std::cerr << "ERROR: In-session options require the client to be "
                   "executed within a session!"
                << std::endl;
      return EXIT_InvocationError;
    }

    ControlClient CC{Client, std::move(Opts.SessionData->SessionName)};
    if (!Client.attached())
    {
      std::cerr << "ERROR: Failed to attach to session '" << CC.sessionName()
                << "'!" << std::endl;
      return EXIT_SystemError;
    }

    if (Opts.DetachRequestLatest)
    {
      CC.requestDetachLatestClient();
    }
    else if (Opts.DetachRequestAll)
    {
      CC.requestDetachAllClients();
    }

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
        std::cerr
          << "ERROR: Failed to establish full connection after enough retries."
          << std::endl;
        return EXIT_SystemError;
      }

      std::clog << "WARNING: Establishing full connection failed:\n\t"
                << FailureReason << std::endl;
      std::clog << "DEBUG: Trying to authenticate with server again..."
                << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  // --------------------- Deduce what session to attach to --------------------
  {
    std::optional<std::vector<SessionData>> Sessions =
      Client.requestSessionList();
    if (!Sessions.has_value())
    {
      std::cerr
        << "ERROR: Receiving the list of sessions from the server failed!"
        << std::endl;
      return EXIT_SystemError;
    }

    std::string DefaultProgram = Opts.Program ? *Opts.Program : defaultShell();
    if (DefaultProgram.empty())
      std::cerr << "WARNING: Failed to figure out what shell is being used, "
                   "and no good defaults are available.\nPlease set the SHELL "
                   "environment variable."
                << std::endl;

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
        std::cerr << "ERROR: When creating a new session, no program set."
                  << std::endl;
        return EXIT_InvocationError;
      }
      Spawn.Arguments = std::move(Opts.ProgramArgs);
      Spawn.Environment["MONOMUX_UNSET"] = std::nullopt;
      Spawn.Environment["MONOMUX_SET"] = "TEST";

      std::optional<std::string> Response =
        Client.requestMakeSession(SessionAction.SessionName, std::move(Spawn));
      if (!Response.has_value() || Response->empty())
      {
        std::cerr << "ERROR: When creating a new session, the creation failed."
                  << std::endl;
        return EXIT_SystemError;
      }
      SessionAction.SessionName = *Response;
      SessionAction.Mode = SessionSelectionResult::Attach;
      // Intended "fallthrough".
    }
    if (SessionAction.Mode == SessionSelectionResult::Attach)
    {
      std::clog << "DEBUG: Attaching to '" << SessionAction.SessionName
                << "'..." << std::endl;
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

  // This below is unclean code.

  // setvbuf(stdin, NULL, _IONBF, 0);
  // setvbuf(stdout, NULL, _IONBF, 0);
  //
  // termios Mode;
  // raw_fd TTY = ::open("/dev/tty", O_RDWR);
  // if (tcgetattr(TTY, &Mode) < 0)
  //   return EXIT_FAILURE;
  // termios NewMode = Mode;
  // NewMode.c_lflag &= ~(ICANON | ECHO);
  //
  // // TODO: Do we need all these flags, really?
  // NewMode.c_iflag &= ~IXON;
  // NewMode.c_iflag &= ~IXOFF;
  // NewMode.c_iflag &= ~ICRNL;
  // NewMode.c_iflag &= ~INLCR;
  // NewMode.c_iflag &= ~IGNCR;
  // NewMode.c_iflag &= ~IMAXBEL;
  // NewMode.c_iflag &= ~ISTRIP;
  //
  // NewMode.c_oflag &= ~OPOST;
  // NewMode.c_oflag &= ~ONLCR;
  // NewMode.c_oflag &= ~OCRNL;
  // NewMode.c_oflag &= ~ONLRET;
  //
  // if (tcsetattr(TTY, TCSANOW, &NewMode) < 0)
  //   return EXIT_SystemError;
  //
  // {
  //   Terminal Term{fd::fileno(stdin), fd::fileno(stdout)};
  //   Client.setTerminal(std::move(Term));
  // }

  // ----------------------------- Be a real client ----------------------------
  Client.loop();

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
      std::cout << "[exited]" << std::endl;
      return EXIT_Success;
    case Client::ServerExit:
      std::cout << "[server exited]" << std::endl;
      return EXIT_Success;
  }
}

} // namespace client
} // namespace monomux
