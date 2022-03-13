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
#include <iostream>

#include <fcntl.h>

namespace monomux
{

/// The type used by system calls dealing with flags.
using flag_t = decltype(O_RDONLY);
/// The file descriptor type on the system.
using raw_fd = decltype(::open("", 0));

inline constexpr raw_fd InvalidFD = -1;

/// This is a smart file descriptor wrapper which will call \p close() on the
/// underyling resource at the end of its life.
class fd // NOLINT(readability-identifier-naming)
{
  raw_fd Handle;

public:
  /// Creates an empty file descriptor that does not wrap anything.
  fd() noexcept : Handle(InvalidFD) {}

  /// Wrap the raw platform resource handle into the RAII object.
  fd(raw_fd Handle) noexcept : Handle(Handle)
  {
    std::clog << "TRACE: FD #" << Handle << " opened." << std::endl;
  }

  fd(fd&& RHS) noexcept : Handle(RHS.release()) {}
  fd& operator=(fd&& RHS) noexcept
  {
    if (this == &RHS)
      return *this;
    Handle = RHS.release();
    return *this;
  }

  /// When the wrapper dies, if the object owned a file descriptor, close it.
  ~fd() noexcept
  {
    if (!has())
      return;
    close(release());
  }

  bool has() const noexcept { return Handle != InvalidFD; }

  /// Convert to the system primitive type.
  operator raw_fd() const noexcept { return Handle; }

  /// Convert to the system primitive type.
  raw_fd get() const noexcept { return Handle; }

  /// Takes the file descriptor from the current object and changes it to not
  /// manage anything.
  [[nodiscard]] raw_fd release() noexcept
  {
    raw_fd R = Handle;
    Handle = InvalidFD;
    return R;
  }

public:
  /// Returns the file descriptor for the standard C I/O object.
  static raw_fd fileno(std::FILE* File);

  /// Closes a \b raw file descriptor.
  static void close(raw_fd FD) noexcept;

  /// Adds the given \p Flag, from \p fcntl() flags, to the flags of the given
  /// file descriptor \p FD.
  static void addStatusFlag(raw_fd FD, flag_t Flag) noexcept;
  /// Removes the given \p Flag, from \p fcntl() flags, from the flags of the
  /// given file descriptor \p FD.
  static void removeStatusFlag(raw_fd FD, flag_t Flag) noexcept;

  /// Adds the given \p Flag, from \p fcntl() flags, to the flags of the given
  /// file descriptor \p FD.
  static void addDescriptorFlag(raw_fd FD, flag_t Flag) noexcept;
  /// Removes the given \p Flag, from \p fcntl() flags, from the flags of the
  /// given file descriptor \p FD.
  static void removeDescriptorFlag(raw_fd FD, flag_t Flag) noexcept;

  /// Shortcut function that sets \p O_NONBLOCK and \p FD_CLOEXEC on a file.
  /// This results in the file set to not block when reading from, and to not
  /// be inherited by child processes in a \p fork() - \p exec() situation.
  static void setNonBlockingCloseOnExec(raw_fd FD) noexcept;
};

} // namespace monomux
