/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <map>
#include <optional>
#include <vector>

#include <sys/epoll.h>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/adt/SmallIndexMap.hpp"
#include "monomux/system/IOEvent.hpp"
#include "monomux/system/fd.hpp"

namespace monomux::system::unix
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
class EPoll : public system::IOEvent
{
  friend class Listener;
  /// Helper RAII object that manages assigning a file descriptor into the
  /// listen-set of an \p epoll(7) structure.
  class Listener
  {
    EPoll& Master;
    fd::raw_fd FDToListenFor;

  public:
    Listener(EPoll& Master, fd::raw_fd FD, bool Incoming, bool Outgoing);
    ~Listener();
  };

public:
  /// Create a new \p epoll(7) structure associated with the current process.
  ///
  /// The structure is initialised to support at most \p EventCount events.
  EPoll(std::size_t EventCount);

  ~EPoll() override;

  [[nodiscard]] std::size_t getScheduledCount() const noexcept override
  {
    return ScheduledResult.size();
  }

  [[nodiscard]] std::size_t getMaxEventCount() const noexcept override
  {
    return Notifications.size();
  }

  [[nodiscard]] std::size_t wait() override;

  /// Retrieve the Nth event.
  [[nodiscard]] const struct ::epoll_event& operator[](std::size_t Index) const
  {
    return at(Index);
  }
  /// Retrieve the Nth event.
  MONOMUX_MEMBER_1(
    struct ::epoll_event&, operator[], [[nodiscard]], , std::size_t, Index);
  /// Retrieve the Nth event.
  [[nodiscard]] const struct ::epoll_event& at(std::size_t Index) const;
  /// Retrieve the Nth event.
  MONOMUX_MEMBER_1(
    struct ::epoll_event&, at, [[nodiscard]], , std::size_t, Index);

  /// Retrieve the file descriptor that fired for the Nth event.
  [[nodiscard]] fd::raw_fd fdAt(std::size_t Index) noexcept;

  [[nodiscard]] EventWithMode eventAt(std::size_t Index) noexcept override;

  void listen(Handle::Raw FD, bool Incoming, bool Outgoing) override;

  void stop(Handle::Raw FD) override;

  void clear() override;

  void schedule(Handle::Raw FD, bool Incoming, bool Outgoing) override;

private:
  /// The file descriptor registered in the system for the event structure.
  fd MasterFD;
  std::map<fd::raw_fd, Listener> Listeners;

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
  std::optional<std::size_t> ScheduleFDNotifiedAtIndex;

  static constexpr std::size_t FDLookupSize = 256;
  /// Contains the events that were manually scheduled by the client before a
  /// call to \p wait(). After \p wait() is called, the events are moved to
  /// the \p ScheduledResult list to be accessed appropriately.
  std::vector<POD<struct ::epoll_event>> ScheduledWaiting;
  /// Map file descriptor values to existing records in the \p ScheduledWaiting
  /// vector. Used only to de-duplicate the same file descriptor being scheduled
  /// more than once.
  SmallIndexMap<decltype(ScheduledWaiting)::iterator,
                FDLookupSize,
                /* StoreInPlace =*/true,
                /* IntrusiveDefaultSentinel =*/true>
    ScheduledWaitingMap;

  [[nodiscard]] bool isValidIndex(std::size_t I) const noexcept;
};

} // namespace monomux::system::unix
