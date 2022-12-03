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
#include <string>

#include <fcntl.h>

#include "monomux/system/Platform.hpp"

namespace monomux::system
{

template <> struct HandleTraits<PlatformTag::UNIX>
{
  /// The file descriptor type on a POSIX system.
  using raw_fd = decltype(::open("", 0));

  /// The file descriptor type on a POSIX system.
  using RawTy = raw_fd;

  /// A magic constant representing the invalid file descriptor.
  static constexpr RawTy Invalid = -1;

  /// Closes a \b raw file descriptor.
  static void close(RawTy FD) noexcept;

  /// Formats the \b raw file descriptor as a string.
  // NOLINTNEXTLINE(readability-identifier-naming)
  static std::string to_string(RawTy FD);
};

} // namespace monomux::system
