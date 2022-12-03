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
#include <algorithm>

#include "monomux/Time.hpp"
#include "monomux/server/ClientData.hpp"
#include "monomux/system/Pipe.hpp"

#include "monomux/server/SessionData.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("server/SessionData")

namespace monomux::server
{

void SessionData::setProcess(
  std::unique_ptr<system::Process>&& Process) noexcept
{
  MainProcess = std::move(Process);
}

system::Handle::Raw SessionData::getIdentifyingHandle() const noexcept
{
  if (!hasProcess() || !getProcess().hasPty())
    return system::PlatformSpecificHandleTraits::Invalid;

  auto& P = const_cast<system::Process&>(getProcess());
  return P.getPty()->raw().get();
}

ClientData* SessionData::getLatestClient() const
{
  MONOMUX_TRACE_LOG(LOG(trace) << "Searching latest active client of \"" << Name
                               << "\"...");
  ClientData* R = nullptr;
  std::optional<decltype(std::declval<ClientData>().lastActive())> Time;
  for (ClientData* C : AttachedClients)
  {
    if (!C->getDataSocket())
      continue;
    MONOMUX_TRACE_LOG(LOG(data)
                      << "\tCandidate client \"" << C->id()
                      << "\" last active at " << formatTime(C->lastActive()));
    auto CTime = C->lastActive();
    if (!Time || *Time < CTime)
    {
      MONOMUX_TRACE_LOG(LOG(data) << "\t\tSelecting \"" << C->id());
      Time = CTime;
      R = C;
    }
  }
  MONOMUX_TRACE_LOG(if (R) LOG(debug)
                      << "\tSelected client \"" << R->id() << '"';
                    else LOG(debug) << "\tNo clients attached";);
  return R;
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

} // namespace monomux::server

#undef LOG
