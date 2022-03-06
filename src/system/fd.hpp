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

#include "CheckedPOSIX.hpp"

#include <iostream>

#include <fcntl.h>
#include <unistd.h>

namespace monomux
{

using flag_t = decltype(O_RDONLY);
using raw_fd = decltype(::open("", 0));
inline constexpr raw_fd InvalidFD = -1;

/// This is a smart file descriptor wrapper which will call \p close() on the
/// underyling resource at the end of its life.
class fd
{
public:
  /// Creates an empty file descriptor that does not wrap anything.
  fd() noexcept : Handle(InvalidFD) {}

  /// Wrap the resource handle into the RAII object.
  fd(raw_fd Handle) noexcept : Handle(Handle)
  {
    std::cerr << "FD " << Handle << " opened." << std::endl;
  }

  fd(fd&& RHS) : Handle(RHS.release()) {}
  fd& operator=(fd&& RHS)
  {
    if (this == &RHS)
      return *this;
    Handle = RHS.release();
    return *this;
  }

  /// Closes a **raw** file descriptor.
  static void close(raw_fd FD) noexcept
  {
    std::cerr << "Closing FD " << FD << "..." << std::endl;
    CheckedPOSIX([FD] { return ::close(FD); }, -1);
  }

  /// When the wrapper dies, if the object owned a file descriptor, close it.
  ~fd() noexcept
  {
    if (!has())
      return;
    close(release());
  }

  bool has() const { return Handle != InvalidFD; }

  /// Convert to the system primitive type.
  operator raw_fd() const { return Handle; }

  /// Convert to the system primitive type.
  raw_fd get() const { return Handle; }

  /// Takes the file descriptor from the current object and changes it to not
  /// manage anything.
  [[nodiscard]] raw_fd release()
  {
    raw_fd R = Handle;
    Handle = InvalidFD;
    return R;
  }

private:
  raw_fd Handle;
};

/// Adds the given \p Flag, from \p fcntl() flags, to the flags of the given
/// file descriptor \p FD.
inline void addStatusFlag(raw_fd FD, flag_t Flag)
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

/// Removes the given \p Flag, from \p fcntl() flags, from the flags of the
/// given file descriptor \p FD.
inline void removeStatusFlag(raw_fd FD, flag_t Flag)
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

/// Adds the given \p Flag, from \p fcntl() flags, to the flags of the given
/// file descriptor \p FD.
inline void addDescriptorFlag(raw_fd FD, flag_t Flag)
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

/// Removes the given \p Flag, from \p fcntl() flags, from the flags of the
/// given file descriptor \p FD.
inline void removeDescriptorFlag(raw_fd FD, flag_t Flag)
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

/// Shortcut function that sets O_NONBLOCK and FD_CLOEXEC on a file.
inline void setNonBlockingCloseOnExec(raw_fd FD)
{
  addStatusFlag(FD, O_NONBLOCK);
  addDescriptorFlag(FD, FD_CLOEXEC);
}

} // namespace monomux
