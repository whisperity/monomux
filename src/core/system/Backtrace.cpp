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
