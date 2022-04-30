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

#include "monomux/adt/POD.hpp"
#include "monomux/system/fd.hpp"

namespace monomux
{

/// More type-safe wrapper over an \p epoll(7) event polling structure.
/// \p epoll(7) works as an I/O event notification system similarly to the
/// hopefully widely known \p select(2) kernel functionality.
///
/// \p EPoll registers a file descriptor internal to the proces which will be
/// notified by the kernel if some of the registered files undergo an I/O
/// change, such as data becoming available on a socket.
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
  EPoll(std::size_t EventCount);

  ~EPoll();

  /// Get the number of events that fired in the last successful \p wait().
  std::size_t getEventCount() const noexcept { return FiredEventCount; }
  std::size_t getMaxEventCount() const noexcept { return Events.size(); }

  /// Blocks and waits until there is a notification that signalled the event
  /// watcher.
  ///
  /// \return The number of events received.
  std::size_t wait();

  /// Retrieve the Nth event.
  struct ::epoll_event& operator[](std::size_t Index)
  {
    return const_cast<struct ::epoll_event&>(
      const_cast<const EPoll*>(this)->at(Index));
  }
  /// Retrieve the Nth event.
  const struct ::epoll_event& operator[](std::size_t Index) const
  {
    return at(Index);
  }
  /// Retrieve the Nth event.
  struct ::epoll_event& at(std::size_t Index)
  {
    return const_cast<struct ::epoll_event&>(
      const_cast<const EPoll*>(this)->at(Index));
  }
  /// Retrieve the Nth event.
  const struct ::epoll_event& at(std::size_t Index) const
  {
    assert(Index < FiredEventCount && "Read past the end of the buffer.");
    return *Events.at(Index);
  }

  /// Retrieve the file descriptor that fired for the Nth event.
  raw_fd fdAt(std::size_t Index) const { return at(Index).data.fd; }

  /// Adds the specified file descriptor \p FD to the event queue. Events will
  /// trigger for \p Incoming (the file is available for reading) or \p Outgoing
  /// (the file is available for writing) operations.
  void listen(raw_fd FD, bool Incoming, bool Outgoing);

  /// Stop listening for changes of \p FD.
  void stop(raw_fd FD);

  /// Stop listening on \b all associated file descriptors.
  void clear();

private:
  std::size_t FiredEventCount = 0;
  fd MasterFD;
  std::map<raw_fd, Listener> Listeners;

  /// Contains the events that fired and triggered a notification.
  std::vector<POD<struct ::epoll_event>> Events;
};

} // namespace monomux
