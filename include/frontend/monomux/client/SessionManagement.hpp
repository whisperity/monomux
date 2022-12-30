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
