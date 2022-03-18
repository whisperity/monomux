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
#include "POD.hpp"

#include <any>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>

namespace monomux
{

/// Wrapper object that allows managing signal handling of a process.
///
/// This class allows client code to register specific callbacks to fire when a
/// signal is received by the running program.
///
/// \warning Signal settings of a process is a \b GLOBAL state! This class
/// exposes a user interface for signal management but puts no effort into
/// tracking changes done by the low-level API: do \b NOT use this class and the
/// low-level API together!
///
/// \see signal(7)
class SignalHandling
{
public:
  using Signal = int;

  /// The number of signals available on the current platform.
  static constexpr std::size_t SignalCount = NSIG;

  /// The number of callbacks that could be registered \e per \e signal.
  static constexpr std::size_t CallbackCount = 4;

  /// The number of objects that might be registered into the configuration
  /// for use in signal handlers.
  static constexpr std::size_t ObjectCount = 16;

  /// The type of the user-implemented signal handlers that can be registered
  /// as a callback.
  ///
  /// \param SigNum The number of the signal that caused the invocation of the
  /// handler.
  /// \param Info Extended data structure containing low-level information about
  /// the received signal.
  /// \param Handling A pointer to the \b GLOBAL signal handling data structure.
  /// This parameter can be used to access registered objects in the callback.
  /// (See \p getObject.)
  using SignalCallback = void(Signal SigNum,
                              ::siginfo_t* Info,
                              const SignalHandling* Handling);

private:
  /// The signal handler callback required by the low-level interface.
  /// This function dispatches to the user-facing registered handlers.
  ///
  /// \param SigNum The number of the signal that caused the invocation of the
  /// handler.
  /// \param Info Extended data structure containing low-level information about
  /// the received signal.
  /// \param Context \b UNUSED! Low-level data structure about the state of the
  /// program when the signal happened. This field is not used.
  static void handler(Signal SigNum, ::siginfo_t* Info, void* Context);

  /// The global object for the signal handler.
  static std::unique_ptr<SignalHandling> Singleton;

  /// A lookup table of callbacks to fire per signal.
  std::array<std::array<std::function<SignalCallback>, CallbackCount>,
             SignalCount>
    Callbacks;

  /// A lookup table of object names.
  std::array<std::string, ObjectCount> ObjectNames;
  /// A lookup table of associated objects.
  std::array<std::any, ObjectCount> Objects;

  /// A lookup table for the signal codes that are registered by \p enable().
  std::array<bool, SignalCount> RegisteredSignals;

public:
  /// Retrieve the \b GLOBAL \p SignalHandling object for the process.
  /// If no such object exists, it will be constructed.
  static SignalHandling& get();

  /// Perform the low-level functions that registers the handler callback of
  /// \p SignalHandling in the kernel for the process, effectively \e enabling
  /// the \b global object to manage signals.
  ///
  /// Calling this function multiple times is \b allowed, and will result in
  /// updating the set of to-be-handled signals on the kernel's side.
  ///
  /// \note Only signals that a callback has been registered for with
  /// \p registerCallback will be enabled in the kernel.
  void enable();

  /// Perform the low-level functions that remove the effects of \p enable().
  void disable();

  /// Registers a callback to fire when \p SigNum is received.
  /// The callback is added at the \b end of the signal queue.
  void registerCallback(Signal SigNum, std::function<SignalCallback> Callback);

  /// Remove all callbacks from the callback list for \p SigNum.
  void clearCallbacks(Signal SigNum);

  /// Remove all callbacks.
  void clearCallbacks() noexcept;

  /// Register the \p Object with \p Name in the global object storage of the
  /// signal handler.
  void registerObject(std::string Name, std::any Object);

  /// Delete the object registered as \p Name, if it is registered.
  void deleteObject(const std::string& Name) noexcept;

  /// Retrieves the object registered with \p Name, if exists.
  ///
  /// \note It is the client code's responsibility to \p std::any_cast the
  /// returned pointer to the appropriate type. The \p SignalHandling instance
  /// does \b NOT manage the types of registered objects.
  std::any* getObject(const char* Name) noexcept;

  /// Retrieves the object registered with \p Name, if exists.
  ///
  /// \note It is the client code's responsibility to \p std::any_cast the
  /// returned pointer to the appropriate type. The \p SignalHandling instance
  /// does \b NOT manage the types of registered objects.
  const std::any* getObject(const char* Name) const noexcept;
};

} // namespace monomux
