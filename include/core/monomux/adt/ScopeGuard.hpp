/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "monomux/adt/UniqueScalar.hpp"

namespace monomux
{

/// A simple scope guard that fires a callback function (in most cases, a
/// lambda passed to the constructor) when constructed, and when destructed.
///
/// Example:
///
///   \code{.cpp}
///   scope_guard RAII{[] { enter(); }, [] { exit(); }};
///   \endcode
template <typename EnterFunction, typename ExitFunction>
struct [[deprecated("Replace by scope_guard")]] ScopeGuard
{
  ScopeGuard(EnterFunction&& Enter, ExitFunction&& Exit) : Exit(Exit)
  {
    Enter();
    Alive = true;
  }

  ~ScopeGuard()
  {
    if (Alive)
      Exit();
  }

  ScopeGuard() = delete;
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard(ScopeGuard&&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;

private:
  bool Alive;
  ExitFunction Exit;
};

} // namespace monomux
