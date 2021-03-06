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
#include <iomanip>

#include <sys/eventfd.h>
#include <unistd.h>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/Event.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Event")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << MasterFD << ": "

namespace monomux
{

EPoll::EPoll(std::size_t EventCount)
{
  Notifications.resize(EventCount);
  ScheduledResult.reserve(EventCount);
  ScheduledWaiting.reserve(EventCount);

  MasterFD = CheckedPOSIXThrow(
    [EventCount] { return ::epoll_create(EventCount); }, "epoll_create()", -1);
  fd::setNonBlockingCloseOnExec(MasterFD.get());

  LOG_WITH_IDENTIFIER(debug) << "Created with " << EventCount << " events";

  ScheduleFD = CheckedPOSIXThrow(
    [] { return ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK); }, "eventfd()", -1);
  LOG_WITH_IDENTIFIER(debug) << "Created eventfd token at " << ScheduleFD;
  listen(ScheduleFD.get(), /* Incoming =*/true, /* Outgoing =*/false);
}

EPoll::~EPoll() { LOG_WITH_IDENTIFIER(debug) << "~EPoll"; }

std::size_t EPoll::wait()
{
  ScheduledResult.clear();
  ScheduleFDNotifiedAtIndex.reset();

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "epoll_wait()...");
  auto MaybeFiredEventCount = CheckedPOSIX(
    [this] {
      return ::epoll_wait(
        MasterFD, &(*Notifications.data()), getMaxEventCount(), -1);
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
  NotificationCount = MaybeFiredEventCount.get();
  if (!ScheduledWaiting.empty())
  {
    // If there are scheduled results, the 'eventfd' will trigger and that
    // will count as a nofication, but this would destroy our calculations.
    --NotificationCount;

    // Consume the scheduled event token.
    POD<std::uint64_t> ScheduledCount;
    CheckedPOSIX(
      [Token = ScheduleFD.get(), &ScheduledCount] {
        return ::read(Token, &ScheduledCount, sizeof(ScheduledCount));
      },
      -1);
    if (ScheduledCount != ScheduledWaiting.size())
      LOG_WITH_IDENTIFIER(debug) << "eventfd_read() -> " << ScheduledCount
                                 << " != expected " << ScheduledWaiting.size();

    // Save where the ScheduleFD was triggered at. The client should not be
    // allowed to directly see that event.
    for (std::size_t I = 0; I < NotificationCount + 1; ++I)
    {
      const struct ::epoll_event& E = **(Notifications.begin() + I);
      if (E.data.fd == ScheduleFD)
      {
        ScheduleFDNotifiedAtIndex.emplace(I);
        break;
      }
    }
  }

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << "epoll_wait()"
                    << " -> " << NotificationCount << " events");

  // Move the events that were scheduled before wait() into the result set.
  ScheduledWaiting.swap(ScheduledResult);
  ScheduledWaitingMap.clear();
  MONOMUX_TRACE_LOG({
    if (!ScheduledResult.empty())
      LOG_WITH_IDENTIFIER(trace)
        << "epoll_wait()"
        << " -> " << ScheduledResult.size() << " scheduled";
  });

  return ScheduledResult.size() + NotificationCount;
}

void EPoll::schedule(raw_fd FD, bool Incoming, bool Outgoing)
{
  auto SetupEvent = [=](struct ::epoll_event& E) {
    E.data.fd = FD;
    if (Incoming)
      E.events |= EPOLLIN;
    if (Outgoing)
      E.events |= EPOLLOUT;
  };

  auto* MaybeIt = ScheduledWaitingMap.tryGet(FD);
  if (!MaybeIt)
  {
    CheckedPOSIX(
      [Token = ScheduleFD.get()] {
        static UniqueScalar<std::uint64_t, 1> One;
        return ::write(Token, &One, sizeof(One));
      },
      -1);

    struct ::epoll_event& E = ScheduledWaiting.emplace_back();
    ScheduledWaitingMap.set(FD, ScheduledWaiting.end() - 1);
    SetupEvent(E);
    return;
  }
  SetupEvent(**MaybeIt);
}

bool EPoll::isValidIndex(std::size_t I) const noexcept
{
  return I < ScheduledResult.size() + NotificationCount;
}

const struct ::epoll_event& EPoll::at(std::size_t Index) const
{
  assert(isValidIndex(Index) && "Read past the end of the buffer.");

  const std::size_t ScheduledCount = ScheduledResult.size();
  if (Index < ScheduledCount)
    // The first set of events appearing to the client should be the
    // manually scheduled ones.
    return *ScheduledResult.at(Index);

  // The rest of the buffer should be taken from the real system result set.
  Index -= ScheduledCount;
  // Skipping (as if never existed) the position where the eventfd(2) trigger
  // arrived.
  if (!ScheduleFDNotifiedAtIndex.has_value() ||
      Index < *ScheduleFDNotifiedAtIndex)
    return *Notifications.at(Index);
  return *Notifications.at(Index + 1);
}

raw_fd EPoll::fdAt(std::size_t Index) noexcept
{
  return isValidIndex(Index) ? at(Index).data.fd : fd::Invalid;
}

EPoll::EventWithMode EPoll::eventAt(std::size_t Index) noexcept
{
  if (!isValidIndex(Index))
    return {fd::Invalid, false, false};

  const struct ::epoll_event& E = at(Index);
  return {E.data.fd,
          (E.events & EPOLLIN) == EPOLLIN,
          (E.events & EPOLLOUT) == EPOLLOUT};
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
  LOG(trace) << Master.MasterFD << ": "
             << "Listen for FD " << FD << "(incoming: " << std::boolalpha
             << Incoming << ", outgoing: " << Outgoing << std::noboolalpha
             << ')';
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
  LOG(trace) << Master.MasterFD << ": "
             << "Stop listening for FD " << FDToListenFor;
}

} // namespace monomux

#undef LOG_WITH_IDENTIFIER
#undef LOG
