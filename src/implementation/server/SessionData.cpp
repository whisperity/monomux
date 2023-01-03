/* SPDX-License-Identifier: LGPL-3.0-only */
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
