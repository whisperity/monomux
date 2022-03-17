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
#include "system/Process.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <string>
#include <utility>

namespace monomux
{
namespace server
{

class ClientData;

/// Encapsulates a running session under the server owning the instance.
class SessionData
{
public:
  SessionData(std::string Name)
    : Name(std::move(Name)), Created(std::chrono::system_clock::now())
  {}

  const std::string& name() const noexcept { return Name; }
  std::chrono::time_point<std::chrono::system_clock>
  whenCreated() const noexcept
  {
    return Created;
  }

  bool hasProcess() const noexcept { return MainProcess.has_value(); }
  void setProcess(Process&& Process) noexcept;
  Process& getProcess() noexcept
  {
    assert(hasProcess());
    return *MainProcess;
  }
  const Process& getProcess() const noexcept
  {
    assert(hasProcess());
    return *MainProcess;
  }

  const std::vector<ClientData*>& getAttachedClients() const noexcept
  {
    return AttachedClients;
  }
  void attachClient(ClientData& Client);
  void removeClient(ClientData& Client) noexcept;

private:
  /// A user-given identifier for the session.
  std::string Name;
  /// The timestamp when the session was spawned.
  std::chrono::time_point<std::chrono::system_clock> Created;

  /// The process (if any) executing in the session.
  ///
  /// \note This is only set when the session is spawned with a process, and
  /// stores no information about the underlying process, outside of Monomux's
  /// control, changing its image via an \p exec() call...
  std::optional<Process> MainProcess;

  /// The list of clients currently attached to this session.
  std::vector<ClientData*> AttachedClients;
};

} // namespace server
} // namespace monomux
