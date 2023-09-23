/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <type_traits>

namespace monomux
{
/// A simple scope guard that fires an optional callback function when it is
/// constructed, and another callback function (usually, a lambda passed to the
/// constructor) when destructed.
///
/// Examples:
///
///   \code{.cpp}
///   scope_guard Cleanup{[] { exit(); }};
///   \endcode
///
///   \code{.cpp}
///   scope_guard RAII{[] { enter(); }, [] { exit(); }};
///   \endcode
template <typename EnterFunction, typename ExitFunction> struct scope_guard
{
  // NOLINTNEXTLINE(google-explicit-constructor)
  scope_guard(ExitFunction&& Exit) noexcept : Alive(true), Exit(Exit) {}
  scope_guard(EnterFunction&& Enter,
              ExitFunction&& Exit) noexcept(noexcept(Enter()))
    : Alive(false), Exit(Exit)
  {
    Enter();
    Alive = true; // NOLINT(cppcoreguidelines-prefer-member-initializer)
  }

  ~scope_guard() noexcept(noexcept(Exit()))
  {
    if (Alive)
      Exit();
    Alive = false;
  }

  scope_guard() = delete;
  scope_guard(const scope_guard&) = delete;
  scope_guard(scope_guard&&) = delete;
  scope_guard& operator=(const scope_guard&) = delete;
  scope_guard& operator=(scope_guard&&) = delete;

private:
  bool Alive;
  ExitFunction Exit;
};

/// A simple scope guard that restores the value of a "captured" variable when
/// the scope is exited.
///
/// Example:
///
///   \code{.cpp}
///   int X = 4;
///   {
///     restore_guard Reset{X};
///     X = 6;
///   }
///   assert(X == 4);
///   \endcode
template <typename Ty> struct restore_guard
{
  // NOLINTNEXTLINE(google-explicit-constructor)
  restore_guard(Ty& Var) noexcept(std::is_copy_constructible_v<Ty>)
    : Address(std::addressof(Var)), Value(Var)
  {}

  ~restore_guard() noexcept(std::is_move_assignable_v<Ty>)
  {
    *Address = std::move(Value);
    Address = nullptr;
  }

  restore_guard() = delete;
  restore_guard(const restore_guard&) = delete;
  restore_guard(restore_guard&&) = delete;
  restore_guard& operator=(const restore_guard&) = delete;
  restore_guard& operator=(restore_guard&&) = delete;

private:
  Ty* Address;
  Ty Value;
};

} // namespace monomux
