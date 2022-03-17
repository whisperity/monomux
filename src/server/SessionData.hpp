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
#include <optional>
#include <string>
#include <utility>

namespace monomux
{
namespace server
{

/// Encapsulates a running session under the server owning the instance.
class SessionData
{
public:
  SessionData(std::string Name) : Name(std::move(Name)) {}

  const std::string& name() const noexcept { return Name; }

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

private:
  std::string Name;
  std::optional<Process> MainProcess;
};

} // namespace server
} // namespace monomux
