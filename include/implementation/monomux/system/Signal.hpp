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
#include <any>
#include <array>
#include <csignal>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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

  /// Helper key for registering the current module's name with
  /// \p registerObject().
  static constexpr char ModuleObjName[] = "Module";

  /// The number of normal (non-realtime) signals available with the current
  /// implementation.
  ///
  /// \note This does \b NOT include realtime signals. This is a design decision
  /// to keep the data structures smaller, as the project does not care about
  /// realtime signals.
  static constexpr std::size_t SignalCount = __SIGRTMIN;
  // The true SIGRTMIN is an extern libc call which would make it not constepr.

  /// The number of callbacks that might be registered \b per \b signal to
  /// the handling structure.
  static constexpr std::size_t CallbackCount = 4;

  /// The number of objects that might be registered into the configuration
  /// for use in signal handlers.
  static constexpr std::size_t ObjectCount = 4;

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

  /// The type of the signal handler callback required by the low-level
  /// interface.
  ///
  /// \param Context \b UNUSED! Low-level data structure about the state of the
  /// program when the signal happened. This field is not used.
  ///
  /// \see SignalCallback
  using KernelSignalHandler = void(Signal SigNum,
                                   ::siginfo_t* Info,
                                   void* Context);

  /// \returns A human-friendly name for the signal \p SigNum, as specified by
  /// the standard.
  static const char* signalName(Signal SigNum) noexcept;

private:
  SignalHandling();

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

  /// A single stored callback for a signal.
  using Callback = std::function<SignalCallback>;
  /// The array of callback \b per \b signal.
  using CallbackArray = std::array<Callback, CallbackCount>;

  /// A lookup table of callbacks for signals.
  std::array<CallbackArray, SignalCount> Callbacks;

  /// A lookup table of object names.
  std::array<std::string, ObjectCount> ObjectNames;
  /// A lookup table of associated objects.
  std::array<std::any, ObjectCount> Objects;

  /// A lookup table for the signal codes that are registered by \p enable().
  std::array<bool, SignalCount> RegisteredSignals;

  /// A lookup table for the signal codes that were masked by \p ignore().
  std::array<bool, SignalCount> MaskedSignals;

public:
  /// Retrieve the \b GLOBAL \p SignalHandling object for the process.
  /// If no such object exists, it will be constructed.
  static SignalHandling& get();

  /// Creates a new \p SignalHandling object.
  ///
  /// \warning As signal handling configuration is a \b GLOBAL state, users
  /// should not construct this class directly!
  ///
  /// \see get()
  static std::unique_ptr<SignalHandling> create();

  SignalHandling(const SignalHandling&) = delete;
  SignalHandling(SignalHandling&&) = delete;
  SignalHandling& operator=(const SignalHandling&) = delete;
  SignalHandling& operator=(SignalHandling&&) = delete;

  /// Perform the low-level functions that registers the handler callback of
  /// \p SignalHandling in the kernel for the process, effectively \e enabling
  /// the \b global object to manage signals.
  ///
  /// Calling this function multiple times is \b allowed, and will result in
  /// updating the set of to-be-handled signals on the kernel's side.
  ///
  /// \note Only signals that a callback has been registered for with
  /// \p registerCallback and \b NOT ignored with \p ignore will be enabled in
  /// the kernel.
  void enable();

  /// \returns whether signal handling (through this object) for \p SigNum had
  /// been enabled.
  bool enabled(Signal SigNum) const noexcept;

  /// Perform the low-level functions that remove the effects of \p enable().
  ///
  /// \note Only signals that are \b NOT ignored with \p ignore will be
  /// restored.
  void disable();

  /// Reset the signal handling configuration for the current process to its
  /// default state.
  void reset();

  /// Sets \p SigNum to be ignored. Signals of this kind will not trigger a
  /// handling if received.
  ///
  /// \see \p SIG_IGN
  void ignore(Signal SigNum);

  /// Removes \p SigNum from the ignore list. If the signal handling was enabled
  /// \b prior to calling \p ignore(), the callbacks will start firing again.
  /// Otherwise, the default signal handling behaviour (as if by calling
  /// \p disable()) will be restored.
  void unignore(Signal SigNum);

  /// Removes all signals from the ignore list.
  void unignore();

  /// Registers a callback to fire when \p SigNum is received.
  /// The callback is added on \b top of the existing callbacks, and will be
  /// fired \b first from all the callbacks.
  void registerCallback(Signal SigNum, std::function<SignalCallback> Callback);

  /// Remove the \b top callback registered for \p SigNum.
  /// If only one callback was registered, behaves as \p clearCallback().
  void clearOneCallback(Signal SigNum);

  /// Remove the callbacks of \p SigNum.
  /// If signal handling had been \p enabled() for the signal beforehand, no
  /// handler will fire.
  void clearCallbacks(Signal SigNum);

  /// Remove all callbacks.
  void clearCallbacks() noexcept;

  /// Remove the callback of \p SigNum and if signal handling had been
  /// \p enabled() for the signal beforehand, reset the default handler.
  void defaultCallback(Signal SigNum);

  /// \returns the \p Indexth callback registered for \p SigNum, or an empty
  /// \p function() if none such are registered.
  std::function<SignalCallback> getCallback(Signal SigNum,
                                            std::size_t Index) const;

  /// \returns the \b top callback registered for \p SigNum, or an empty
  /// \p function() if no callbacks are registered.
  std::function<SignalCallback> getOneCallback(Signal SigNum) const;

  /// Register the \p Object with \p Name in the global object storage of the
  /// signal handler.
  void registerObject(std::string Name, std::any Object);

  /// Delete the object registered as \p Name, if it is registered.
  void deleteObject(const std::string& Name) noexcept;

  /// Delete all objects that are registered.
  void deleteObjects() noexcept;

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
