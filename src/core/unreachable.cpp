/* SPDX-License-Identifier: LGPL-3.0-only */
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include "monomux/unreachable.hpp"

namespace monomux::_detail
{

[[noreturn]] void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
unreachable_impl(const char* Msg, const char* File, std::size_t LineNo)
{
  (void)fprintf(stderr, "FATAL! UNREACHABLE executed");
  if (File)
    (void)fprintf(stderr, " at %s:%zu", File, LineNo);

  if (Msg)
    (void)fprintf(stderr, ": %s!\n", Msg);
  else
    (void)fprintf(stderr, "!\n");

  // [[noreturn]]
  // Use std::abort() primarily so we may still fire a signal handler that
  // dumps the stack trace.
  std::abort();
  std::_Exit(-SIGILL);
  // Really do *everything* to kill the process!
  __builtin_trap();
}

} // namespace monomux::_detail
