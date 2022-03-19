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
#include "SessionData.hpp"
#include "system/Pipe.hpp"

#include <iostream>

namespace monomux
{
namespace server
{

void SessionData::setProcess(Process&& Process) noexcept
{
  std::clog << "DEBUG: Setting process for session " << Name << std::endl;
  MainProcess.reset();
  MainProcess.emplace(std::move(Process));
}

std::string SessionData::readOutput(std::size_t Size)
{
  if (!hasProcess() || !getProcess().hasPty())
    return {};
  bool Success;
  std::string Data = Pipe::read(getProcess().getPty()->raw(), Size, &Success);
  return Success ? Data : "";
}

std::size_t SessionData::sendInput(std::string_view Data)
{
  if (!hasProcess() || !getProcess().hasPty())
    return 0;
  return Pipe::write(getProcess().getPty()->raw(), Data);
}

void SessionData::attachClient(ClientData& Client)
{
  AttachedClients.emplace_back(&Client);
}

void SessionData::removeClient(ClientData& Client) noexcept
{
  for (auto It = AttachedClients.begin(); It != AttachedClients.end(); ++It)
    if (*It == &Client)
    {
      It = AttachedClients.erase(It);
      break;
    }
}

} // namespace server
} // namespace monomux
