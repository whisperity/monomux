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
#include <csignal>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/system/Platform.hpp"

namespace monomux::system
{

class SignalHandling;

template <> struct SignalTraits<PlatformTag::UNIX>
{
  /// Type alias for the raw signal identifier type on the platform.
  using RawTy = argument_t<1, decltype(::signal)>;
  using Signal = RawTy;

  // The true SIGRTMIN is an extern libc call which would make it not constexpr.
  /// Number of signals to consider.
  static constexpr std::size_t Count = __SIGRTMIN;

  /// The type of the user-implemented signal handlers that can be registered
  /// as a callback.
  ///
  /// \param SigNum The number of the signal that caused the invocation of the
  /// handler.
  /// \param Info Extended data structure containing low-level information about
  /// the received signal.
  /// \param Handling A pointer to the \b GLOBAL signal handling data structure.
  /// This parameter can be used to access registered objects in the callback.
  /// (See \p getObject in \p SignalHandling.)
  using HandlerTy = void(Signal SigNum,
                         ::siginfo_t* Info,
                         const SignalHandling* Handling);

  // FIXME: Expose the header of a signal handler signature as a convenience
  // macro.

  /// The type of the signal handler callback required by the low-level
  /// interface.
  ///
  /// \param Context \b UNUSED! Low-level data structure about the state of the
  /// program when the signal happened. This field is not used.
  ///
  /// \see HandlerTy
  using PrimitiveSignalHandler = void(Signal SigNum,
                                      ::siginfo_t* Info,
                                      void* Context);


  /// The signal handler callback required by the low-level interface.
  /// This function dispatches to the user-facing registered handlers.
  ///
  /// \param SigNum The number of the signal that caused the invocation of the
  /// handler.
  /// \param Info Extended data structure containing low-level information about
  /// the received signal.
  /// \param Context \b UNUSED! Low-level data structure about the state of the
  /// program when the signal happened. This field is not used.
  ///
  /// \see signal(7)
  /// \see sigaction(2)
  static void signalDispatch(Signal SigNum, ::siginfo_t* Info, void* Context);

  static_assert(
    std::is_same_v<decltype(signalDispatch), PrimitiveSignalHandler>,
    "Low-level signal handler callback's type invalid.");

  /// Registers in the OS that signal \p S should be handled by \p
  /// signalDispatch.
  static void setSignalHandled(Signal SigNum);
  /// Registers in the OS that signal \p S should use the default handling.
  ///
  /// \see SIG_DFL
  static void setSignalDefault(Signal SigNum);
  /// Registers in the OS that signal \p S should be ignored.
  ///
  /// \see SIG_IGN
  static void setSignalIgnored(Signal SigNum);
};

} // namespace monomux::system
