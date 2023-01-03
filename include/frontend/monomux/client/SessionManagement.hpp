/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <string>
#include <vector>

#include "monomux/FrontendExitCode.hpp"
#include "monomux/client/Main.hpp"

namespace monomux::client
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

FrontendExitCode sessionCreateOrAttach(Options& Opts);

} // namespace monomux::client
