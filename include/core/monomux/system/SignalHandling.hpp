/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <any>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/system/CurrentPlatform.hpp"
#include "monomux/system/SignalTraits.hpp"

namespace monomux::system
{

using PlatformSpecificSignalTraits = SignalTraits<CurrentPlatform>;

/// Wrapper object that allows managing signal handling of a process.
///
/// This class allows client code to register specific callbacks to fire when a
/// signal is received by the running program.
///
/// \warning Signal settings of a process is a \b GLOBAL state! This class
/// exposes a user interface for signal management but puts no effort into
/// tracking changes done by OS-provided low-level API: do \b NOT use this
/// class and the low-level APIs together!
class SignalHandling
{
public:
  using Signal = PlatformSpecificSignalTraits::RawTy;

  /// Helper key for registering the current module's name with
  /// \p registerObject().
  static constexpr char ModuleObjName[] = "Module";

  /// The number of normal (non-realtime) signals available with the current
  /// implementation.
  ///
  /// \note This does \b NOT include realtime signals. This is a design decision
  /// to keep the data structures smaller, as the project does not care about
  /// realtime signals.
  static constexpr std::size_t SignalCount =
    PlatformSpecificSignalTraits::Count;

  /// The number of callbacks that might be registered \b per \b signal to
  /// the handling structure.
  static constexpr std::size_t CallbackCount = 4;

  /// The number of objects that might be registered into the configuration
  /// for use in signal handlers.
  static constexpr std::size_t ObjectCount = 4;

  /// The type of the signal handler that the clients of this class must
  /// implement.
  using SignalCallback = PlatformSpecificSignalTraits::HandlerTy;

/// Generate the signature for a signal handler function of the given name.
#define MONOMUX_SIGNAL_HANDLER(NAME) MONOMUX_PLATFORM_SIGNAL_HANDLER_SIG(NAME)

  /// \returns A human-friendly name for the signal \p SigNum, as specified by
  /// the standard.
  static const char* signalName(Signal SigNum) noexcept;

private:
  SignalHandling();

  friend PlatformSpecificSignalTraits;

  /// The global object for the signal handler, that should be instantiated
  /// only once per process. Automatically spawned and returned by the first
  /// call to \p get().
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
  [[nodiscard]] static std::unique_ptr<SignalHandling> create();

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
  [[nodiscard]] bool enabled(Signal SigNum) const noexcept;

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
  [[nodiscard]] std::function<SignalCallback>
  getCallback(Signal SigNum, std::size_t Index) const;

  /// \returns the \b top callback registered for \p SigNum, or an empty
  /// \p function() if no callbacks are registered.
  [[nodiscard]] std::function<SignalCallback>
  getOneCallback(Signal SigNum) const;

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
  [[nodiscard]] const std::any* getObject(const char* Name) const noexcept;

  /// Retrieves the object registered with \p Name, if exists.
  ///
  /// \note It is the client code's responsibility to \p std::any_cast the
  /// returned pointer to the appropriate type. The \p SignalHandling instance
  /// does \b NOT manage the types of registered objects.
  MONOMUX_MEMBER_1(
    std::any*, getObject, [[nodiscard]], noexcept, const char*, Name);

  /// Retrieves the object registered with \p Name, if it exists and is of
  /// type \p T.
  template <typename T>
  [[nodiscard]] const T* getObjectAs(const char* Name) const noexcept
  {
    const std::any* Obj = getObject(Name);
    if (!Obj)
      return nullptr;

    return std::any_cast<T>(Obj);
  }

  /// Retrieves the object registered with \p Name, if it exists and is of
  /// type \p T.
  MONOMUX_MEMBER_T1_1(
    T*, getObjectAs, [[nodiscard]], noexcept, typename, T, const char*, Name);
};

} // namespace monomux::system
