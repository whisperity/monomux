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
#include <cassert>
#include <map>
#include <vector>

#include <sys/epoll.h>

#include "monomux/adt/MemberFunctionHelper.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/adt/SmallIndexMap.hpp"
#include "monomux/system/fd.hpp"

namespace monomux
{

/// A type-safe wrapper over an \p epoll(7) event polling structure.
/// \p epoll(7) works as an I/O event notification system similarly to the
/// hopefully widely known \p select(2) kernel functionality.
///
/// \p EPoll registers a file descriptor internal to the proces which will be
/// notified by the kernel if some of the registered files undergo an I/O
/// change, such as data becoming available on a socket.
///
/// Using \p eventfd(2), this implementation is also capable of having events
/// crafted by clients appear as if they were created by the kernel.
class EPoll
{
  friend class Listener;
  /// Helper RAII object that manages assigning a file descriptor into the
  /// listen-set of an \p epoll(7) structure.
  class Listener
  {
    EPoll& Master;
    raw_fd FDToListenFor;

  public:
    Listener(EPoll& Master, raw_fd FD, bool Incoming, bool Outgoing);
    ~Listener();
  };

public:
  /// Create a new \p epoll(7) structure associated with the current process.
  ///
  /// The structure is initialised to support at most \p EventCount events.
  EPoll(std::size_t EventCount);

  ~EPoll();

  /// Get the number of events that fired in the last successful \p wait().
  std::size_t getEventCount() const noexcept { return NotificationCount; }
  /// Get the number of events that were manually scheduled by the client in the
  /// last successful \p wait().
  std::size_t getScheduledCount() const noexcept
  {
    return ScheduledResult.size();
  }

  std::size_t getMaxEventCount() const noexcept { return Notifications.size(); }

  /// Blocks and waits until there is a notification that signalled the event
  /// watcher.
  ///
  /// \return The number of events received, either from the system or by
  /// manual scheduling.
  std::size_t wait();

  /// Retrieve the Nth event.
  const struct ::epoll_event& operator[](std::size_t Index) const
  {
    return at(Index);
  }
  /// Retrieve the Nth event.
  MEMBER_FN_NON_CONST_1(struct ::epoll_event&, operator[], std::size_t, Index);
  /// Retrieve the Nth event.
  const struct ::epoll_event& at(std::size_t Index) const;
  /// Retrieve the Nth event.
  MEMBER_FN_NON_CONST_1(struct ::epoll_event&, at, std::size_t, Index);

  /// Retrieve the file descriptor that fired for the Nth event.
  raw_fd fdAt(std::size_t Index) noexcept;

  struct EventWithMode
  {
    raw_fd FD;
    bool Incoming;
    bool Outgoing;
  };
  /// Retrieve the Nth event.
  EventWithMode eventAt(std::size_t Index) noexcept;

  /// Adds the specified file descriptor \p FD to the event queue. Events will
  /// trigger for \p Incoming (the file is available for reading) or \p Outgoing
  /// (the file is available for writing) operations.
  void listen(raw_fd FD, bool Incoming, bool Outgoing);

  /// Stop listening for changes of \p FD.
  void stop(raw_fd FD);

  /// Stop listening on \b all associated file descriptors.
  void clear();

  /// Explicitly schedule the file descriptor \p FD to appear in the event queue
  /// even if the system generates no event notification for it.
  ///
  /// \p Incoming and \p Outgoing decides which flag(s) the event will appear
  /// as. Scheduled events are placed \b before system notifications in the
  /// result \b after a call to \p wait(), but do not \e override system
  /// results. A file descriptor both "hand-scheduled" and system notified will
  /// appear twice in the result array.
  void schedule(raw_fd FD, bool Incoming, bool Outgoing);

private:
  std::size_t NotificationCount = 0;
  /// The file descriptor registered in the system for the event structure.
  fd MasterFD;
  std::map<raw_fd, Listener> Listeners;

  /// Contains the events that fired and triggered a notification from the
  /// system.
  std::vector<POD<struct ::epoll_event>> Notifications;
  /// Contains the events that were manually scheduled before the most recent
  /// \p wait() call.
  std::vector<POD<struct ::epoll_event>> ScheduledResult;

  /// The file descriptor registered in the system for the manually scheduled
  /// event callbacks.
  fd ScheduleFD;
  /// Contains the index in the \p Notifications vector, after a successful call
  /// to \p wait(), where the \p ScheduleFD's notification was placed.
  std::size_t ScheduleFDNotifiedAtIndex;

  static const std::size_t FDLookupSize = 256;
  /// Contains the events that were manually scheduled by the client before a
  /// call to \p wait(). After \p wait() is called, the events are moved to
  /// the \p ScheduledResult list to be accessed appropriately.
  std::vector<POD<struct ::epoll_event>> ScheduledWaiting;
  SmallIndexMap<decltype(ScheduledWaiting)::iterator,
                FDLookupSize,
                /* StoreInPlace =*/true,
                /* IntrusiveDefaultSentinel =*/true>
    ScheduledWaitingMap;

  bool isValidIndex(std::size_t I) const noexcept;
};

} // namespace monomux
