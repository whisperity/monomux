/* SPDX-License-Identifier: GPL-3.0-only */
#include <iostream>

#include "monomux/Time.hpp"

#include "monomux/client/SessionManagement.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("client/SessionManagement")

namespace monomux::client
{

void emplaceDefaultProgram(Options& Opts)
{
  const bool HasReceivedProgramToStart =
    Opts.Program && !Opts.Program->Program.empty();
  std::string DefaultProgram = HasReceivedProgramToStart
                                 ? Opts.Program->Program
                                 : system::Platform::defaultShell();
  if (DefaultProgram.empty())
    LOG(warn) << "Failed to figure out what shell is being used, and no good "
                 "defaults are available.\nPlease set the SHELL environment "
                 "variable.";
  if (!HasReceivedProgramToStart)
  {
    if (!Opts.Program)
      Opts.Program.emplace(system::Process::SpawnOptions{});
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

FrontendExitCode sessionCreateOrAttach(Options& Opts)
{
  Client& Client = *Opts.Connection;

  std::optional<std::vector<SessionData>> Sessions =
    Client.requestSessionList();
  if (!Sessions.has_value())
  {
    LOG(fatal) << "Receiving the list of sessions from the server failed!";
    return FrontendExitCode::SystemError;
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
    return FrontendExitCode::Success;
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
      return FrontendExitCode::SystemError;
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
        return FrontendExitCode::SystemError;
      }
    }

    LOG(debug) << "Attaching to \"" << SessionAction.SessionName << "\"...";
    bool Attached = Client.requestAttach(std::move(SessionAction.SessionName));
    if (!Attached)
    {
      std::cerr << "ERROR: Server reported failure when attaching."
                << std::endl;
      return FrontendExitCode::SystemError;
    }
  }

  return FrontendExitCode::Success;
}

} // namespace monomux::client

#undef LOG
