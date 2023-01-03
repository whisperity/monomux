/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include <fcntl.h>

#include "monomux/system/Platform.hpp"

namespace monomux::system
{

template <> struct HandleTraits<PlatformTag::Unix>
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
  [[nodiscard]] static std::string to_string(RawTy FD);
};

} // namespace monomux::system
