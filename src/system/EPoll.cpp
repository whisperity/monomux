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
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/EPoll.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/epoll")

namespace monomux
{

EPoll::EPoll(std::size_t EventCount)
{
  Events.resize(EventCount);

  MasterFD = CheckedPOSIXThrow(
    [EventCount] { return ::epoll_create(EventCount); }, "epoll_create()", -1);
  fd::setNonBlockingCloseOnExec(MasterFD.get());

  LOG(debug) << MasterFD << ": created with " << EventCount << " events";
}

EPoll::~EPoll() { LOG(debug) << MasterFD << ": destroyed"; }

std::size_t EPoll::wait()
{
  MONOMUX_TRACE_LOG(LOG(trace) << "epoll_wait()...");
  auto MaybeFiredEventCount = CheckedPOSIX(
    [this] {
      return ::epoll_wait(MasterFD, &(*Events.data()), Events.size(), -1);
    },
    -1);
  if (!MaybeFiredEventCount)
  {
    std::error_code EC = MaybeFiredEventCount.getError();
    if (EC == std::errc::interrupted /* EINTR */)
      // Interrupting epoll_wait() is not an issue.
      return 0;
    throw std::system_error{EC, "epoll_wait()"};
  }
  FiredEventCount = MaybeFiredEventCount.get();
  MONOMUX_TRACE_LOG(LOG(trace)
                    << MasterFD << ": " << FiredEventCount << " events hit.");
  return FiredEventCount;
}

raw_fd EPoll::fdAt(std::size_t Index)
{
  return Index < Events.size() ? at(Index).data.fd : fd::Invalid;
}

void EPoll::listen(raw_fd FD, bool Incoming, bool Outgoing)
{
  Listeners.try_emplace(FD, *this, FD, Incoming, Outgoing);
}

void EPoll::stop(raw_fd FD)
{
  auto It = Listeners.find(FD);
  if (It == Listeners.end())
    return;
  Listeners.erase(It);
}

void EPoll::clear()
{
  for (auto It = Listeners.begin(); It != Listeners.end();)
    It = Listeners.erase(It);
}

EPoll::Listener::Listener(EPoll& Master,
                          raw_fd FD,
                          bool Incoming,
                          bool Outgoing)
  : Master(Master), FDToListenFor(FD)
{
  POD<struct ::epoll_event> Control;
  Control->data.fd = FD;
  Control->events = EPOLLHUP | EPOLLRDHUP;
  if (Incoming)
    Control->events |= EPOLLIN;
  if (Outgoing)
    Control->events |= EPOLLOUT;

  CheckedPOSIXThrow(
    [&Master, &Control, FD] {
      return ::epoll_ctl(Master.MasterFD, EPOLL_CTL_ADD, FD, &Control);
    },
    "epoll_ctl registering file",
    -1);
  LOG(trace) << Master.MasterFD << ": listen for FD " << FD;
}

EPoll::Listener::~Listener()
{
  if (FDToListenFor == fd::Invalid)
    return;

  POD<struct ::epoll_event> Control;
  CheckedPOSIX(
    [this, &Control] {
      return ::epoll_ctl(
        Master.MasterFD, EPOLL_CTL_DEL, FDToListenFor, &Control);
    },
    -1);
  LOG(trace) << Master.MasterFD << ": stop listening for FD " << FDToListenFor;
}

} // namespace monomux

#undef LOG
