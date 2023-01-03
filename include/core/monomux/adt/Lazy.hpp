/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <functional>
#include <type_traits>

#include "monomux/adt/POD.hpp"

namespace monomux
{

/// Implements a simple, lazy-initialised wrapper of \p T which will construct
/// the object on the first access by running \p EnterFunction.
template <class T, typename EnterFunction = std::function<void()>> struct Lazy
{
private:
  static constexpr bool HasCustomConstructor =
    std::is_same_v<EnterFunction, std::function<void()>>;
  static constexpr bool IsNoexceptConstruction =
    !HasCustomConstructor
      ? std::is_nothrow_default_constructible_v<T>
      : std::is_nothrow_constructible_v<T, std::invoke_result_t<EnterFunction>>;

  void wipeStorage() noexcept { detail::memset_manual(Storage, 0, sizeof(T)); }

public:
  /// Constructs a \p Lazy object that will default-initialise the underlying
  /// \p T instance.
  Lazy() noexcept { wipeStorage(); }
  /// Constructs a \p Lazy object that will initialise the underlying instance
  /// by executing the \p Enter function.
  Lazy(EnterFunction&& Enter) noexcept(
    std::is_nothrow_move_constructible_v<EnterFunction>)
    : EnterFn(std::move(Enter))
  {
    wipeStorage();
  }

  Lazy(const Lazy& RHS) : Alive(RHS.Alive), EnterFn(RHS.EnterFn)
  {
    if (Alive)
      new (Storage) T{RHS.get()};
  }
  Lazy(Lazy&&) = delete;
  Lazy& operator=(const Lazy&) = delete;
  Lazy& operator=(Lazy&&) = delete;
  /// Destroys the underlying \p T instance.
  ~Lazy()
  {
    reinterpret_cast<T*>(Storage)->~T();
    wipeStorage();
    Alive = false;
  }

  /// \returns the underlying instance.
  ///
  /// If it is not constructed yet, it is first constructed by calling the
  /// \p EnterFunction, if provided, or otherwise default constructed.
  T& get() noexcept(IsNoexceptConstruction)
  {
    if (!Alive)
    {
      if constexpr (HasCustomConstructor)
        new (Storage) T{EnterFn()};
      else
        new (Storage) T{};

      Alive = true;
    }

    return *reinterpret_cast<T*>(Storage);
  }

private:
  bool Alive = false;
  alignas(T) char Storage[sizeof(T)];
  EnterFunction EnterFn;
};

/// Helper function that automatically deduces the type of the \p Lazy instance
/// based on the function (usually a lambda) given to it.
///
/// Example:
///
///   \code{.cpp}
///   auto MyLazy = makeLazy([]() -> some_type { return some_type_maker(); });
///   \endcode
template <typename EnterFunction>
[[nodiscard]] auto makeLazy(EnterFunction&& Enter) noexcept(
  std::is_nothrow_move_constructible_v<EnterFunction>)
{
  return Lazy<std::invoke_result_t<EnterFunction>, EnterFunction>(
    std::forward<EnterFunction>(Enter));
}

} // namespace monomux
