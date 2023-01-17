/* SPDX-License-Identifier: LGPL-3.0-only */
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include "monomux/unreachable.hpp"

namespace monomux::detail
{

[[noreturn]] void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
unreachable_impl(const char* Msg, const char* File, std::size_t LineNo)
{
  /* NOLINTBEGIN(cppcoreguidelines-pro-type-vararg) */
  (void)std::fprintf(stderr, "FATAL! UNREACHABLE executed");
  if (File)
    (void)std::fprintf(stderr, " at %s:%zu", File, LineNo);

  if (Msg)
    (void)std::fprintf(stderr, ": %s!\n", Msg);
  else
    (void)std::fprintf(stderr, "!\n");
  /* NOLINTEND(cppcoreguidelines-pro-type-vararg) */

  // [[noreturn]]
  // Use std::abort() primarily so we may still fire a signal handler that
  // dumps the stack trace.
  std::abort();
  std::_Exit(-SIGILL);
  // Really do *everything* to kill the process!
  __builtin_trap();
}

} // namespace monomux::detail

extern "C" [[noreturn]] void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
monomux_unreachable_impl(const char* Msg, const char* File, std::size_t LineNo)
{
  monomux::detail::unreachable_impl(Msg, File, LineNo);
}
