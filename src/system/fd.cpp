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
#include <sys/resource.h>
#include <unistd.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/fd.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/fd")

namespace monomux
{

std::size_t fd::maxNumFDs()
{
  POD<struct ::rlimit> Limits;
  CheckedPOSIXThrow([&Limits] { return ::getrlimit(RLIMIT_NOFILE, &Limits); },
                    "getrlimit()",
                    -1);
  if (Limits->rlim_cur == RLIM_INFINITY)
    return -1;
  return Limits->rlim_cur;
}

fd::raw_fd fd::fileno(std::FILE* File)
{
  return CheckedPOSIXThrow([File] { return ::fileno(File); }, "fileno()", -1);
}

fd::fd(raw_fd Handle) noexcept : Handle(Handle)
{
  MONOMUX_TRACE_LOG(LOG(debug) << "FD #" << Handle << " opened.");
}

fd fd::dup(raw_fd Handle)
{
  raw_fd DupHandle =
    CheckedPOSIXThrow([Handle] { return ::dup(Handle); }, "dup()", -1);
  return fd{DupHandle};
}

void fd::close(raw_fd FD) noexcept
{
  MONOMUX_TRACE_LOG(LOG(debug) << "Closing FD #" << FD << "...");
  CheckedPOSIX([FD] { return ::close(FD); }, -1);
}

void fd::addStatusFlag(raw_fd FD, flag_t Flag) noexcept
{
  flag_t FlagsNow;
  CheckedPOSIX(
    [FD, &FlagsNow] {
      FlagsNow = ::fcntl(FD, F_GETFL);
      return FlagsNow;
    },
    -1);

  FlagsNow |= Flag;

  CheckedPOSIX([FD, &FlagsNow] { return ::fcntl(FD, F_SETFL, FlagsNow); }, -1);
}

void fd::removeStatusFlag(raw_fd FD, flag_t Flag) noexcept
{
  flag_t FlagsNow;
  CheckedPOSIX(
    [FD, &FlagsNow] {
      FlagsNow = ::fcntl(FD, F_GETFL);
      return FlagsNow;
    },
    -1);

  FlagsNow &= (~Flag);

  CheckedPOSIX([FD, &FlagsNow] { return ::fcntl(FD, F_SETFL, FlagsNow); }, -1);
}

void fd::addDescriptorFlag(raw_fd FD, flag_t Flag) noexcept
{
  flag_t FlagsNow;
  CheckedPOSIX(
    [FD, &FlagsNow] {
      FlagsNow = ::fcntl(FD, F_GETFD);
      return FlagsNow;
    },
    -1);

  FlagsNow |= Flag;

  CheckedPOSIX([FD, &FlagsNow] { return ::fcntl(FD, F_SETFD, FlagsNow); }, -1);
}

void fd::removeDescriptorFlag(raw_fd FD, flag_t Flag) noexcept
{
  flag_t FlagsNow;
  CheckedPOSIX(
    [FD, &FlagsNow] {
      FlagsNow = ::fcntl(FD, F_GETFD);
      return FlagsNow;
    },
    -1);

  FlagsNow &= (~Flag);

  CheckedPOSIX([FD, &FlagsNow] { return ::fcntl(FD, F_SETFD, FlagsNow); }, -1);
}

void fd::setNonBlockingCloseOnExec(raw_fd FD) noexcept
{
  fd::addStatusFlag(FD, O_NONBLOCK);
  fd::addDescriptorFlag(FD, FD_CLOEXEC);
}

} // namespace monomux
