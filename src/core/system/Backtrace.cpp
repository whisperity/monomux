/* SPDX-License-Identifier: LGPL-3.0-only */
#include "monomux/system/CurrentPlatform.hpp"

#ifndef MONOMUX_CURRENT_PLATFORM_SUPPORTS_BACKTRACE

#include <iostream>

#include "monomux/system/Backtrace.hpp"

namespace monomux::system
{

namespace
{

void printBacktraceNotSupported(std::ostream& OS)
{
  OS << '\n'
     << MONOMUX_FEED_PLATFORM_NOT_SUPPORTED_MESSAGE << "printing a backtrace."
     << '\n';
}

} // namespace

void printBacktrace(std::ostream& OS, const Backtrace& /*Trace*/)
{
  printBacktraceNotSupported(OS);
}

void printBacktrace(std::ostream& OS, bool /*Prettify*/)
{
  printBacktraceNotSupported(OS);
}

} // namespace monomux::system

#endif /* MONOMUX_CURRENT_PLATFORM_SUPPORTS_BACKTRACE */
