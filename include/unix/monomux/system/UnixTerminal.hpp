/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>

#include <termios.h>

#include "monomux/adt/Atomic.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/system/SignalHandling.hpp"
#include "monomux/system/UnixPipe.hpp"
#include "monomux/system/fd.hpp"

namespace monomux::system::unix
{

/// Exposes low-level calls that are used to switch the real terminal device
/// associated with a user-facing client between various modes and to interface
/// with the low-level \p termios system.
///
/// \see termios(3)
class Terminal : public std::enable_shared_from_this<Terminal>
{
public:
  /// 2D size.
  struct Size
  {
    unsigned short Rows, Columns;
  };

  [[nodiscard]] static std::shared_ptr<Terminal> create(fd::raw_fd In,
                                                        fd::raw_fd Out);

  /// Sets the \p Terminal underlying device up for raw mode where every
  /// action is individually captured unbuffered.
  ///
  /// Conventionally, the input streams of terminals buffer for a line,
  /// because the initial behaviour is to accept command prompts.
  void setRawMode();

  /// Undoes the actions of \p setRawMode() and resets the \p Terminal
  /// underlying device to the configuration it was in when \p this was
  /// constructed.
  void setOriginalMode();

  [[nodiscard]] Size getSize() const;
  [[nodiscard]] bool hasSizeChangedExternally() const noexcept
  {
    return SizeChanged.load();
  }
  void notifySizeChanged() const noexcept { SizeChanged.store(true); }
  void clearSizeChanged() noexcept { SizeChanged.store(false); }

  /// Creates the associated data structures that allows \p Handling to callback
  /// \p SIGWINCH (window size changed) events to the \p Terminal instances, and
  /// registers the current \p Terminal into those data structures.
  ///
  /// \note This function does \b NOT \p enable() the signal handling, just
  /// prepares the handler.
  void setupListenForSizeChangeSignal(system::SignalHandling& Handling);
  /// Removes the current \p Terminal from the signal handling data structures.
  /// (If the current terminal was the last one to be removed, cleans up the
  /// data structures associated with the handling logic.)
  void teardownListenForSizeChangeSignal(system::SignalHandling& Handling);

private:
  Terminal(fd::raw_fd In, fd::raw_fd Out);

  fd::raw_fd In, Out;
  POD<struct ::termios> TerminalSettings;

  /// Whether a signal interrupt indicated that the window size of the client
  /// had changed.
  mutable Atomic<bool> SizeChanged;
};

} // namespace monomux::system::unix
