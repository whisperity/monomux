/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>

#include "monomux/system/Handle.hpp"

namespace monomux::system
{

/// Implements the wrapping of the OS-primitives for I/O event polling features
/// making clients able to detect read and write operations, availability, or
/// error conditions of a file handle.
class IOEvent
{
public:
  virtual ~IOEvent() = default;

  /// Get the number of events that fired in the last successful \p wait().
  [[nodiscard]] std::size_t getEventCount() const noexcept
  {
    return NotificationCount;
  }
  /// Get the number of events that were manually scheduled by the client in the
  /// last successful \p wait().
  [[nodiscard]] virtual std::size_t getScheduledCount() const noexcept = 0;

  [[nodiscard]] virtual std::size_t getMaxEventCount() const noexcept = 0;

  /// Blocks and waits until there is a notification that signalled the event
  /// watcher.
  ///
  /// \return The number of events received, either from the system or by
  /// manual scheduling.
  [[nodiscard]] virtual std::size_t wait() = 0;

  struct EventWithMode
  {
    Handle::Raw FD;
    bool Incoming;
    bool Outgoing;
  };
  /// Retrieve the Nth event.
  [[nodiscard]] virtual EventWithMode eventAt(std::size_t Index) noexcept = 0;

  /// Adds the specified file descriptor \p FD to the event queue. Events will
  /// trigger for \p Incoming (the file is available for reading) or \p Outgoing
  /// (the file is available for writing) operations.
  virtual void listen(Handle::Raw FD, bool Incoming, bool Outgoing) = 0;

  /// Stop listening for changes of \p FD.
  virtual void stop(Handle::Raw FD) = 0;

  /// Stop listening on \b all associated file descriptors.
  virtual void clear() = 0;

  /// Explicitly schedule the file descriptor \p FD to appear in the event queue
  /// even if the system generates no event notification for it.
  ///
  /// \p Incoming and \p Outgoing decides which flag(s) the event will appear
  /// as. Scheduled events are placed \b before system notifications in the
  /// result \b after a call to \p wait(), but do not \e override system
  /// results. A file descriptor both "hand-scheduled" and system notified will
  /// appear twice in the result array.
  virtual void schedule(Handle::Raw FD, bool Incoming, bool Outgoing) = 0;

protected:
  IOEvent() = default;

  std::size_t NotificationCount = 0;
};

} // namespace monomux::system
