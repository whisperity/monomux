/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "monomux/system/Handle.hpp"

#include <cstdio>

namespace monomux::system::unix
{

/// This is a smart file descriptor wrapper which will call \p close() on the
/// underyling resource at the end of its life.
///
/// \note \p fd is not a polymorphic class. Due to using the \p HandleTraits
/// detail implementation, it is always safe to assign an \p fd instance to a
/// \p Handle instance.
class fd : public Handle // NOLINT(readability-identifier-naming)
{
public:
  using Traits = HandleTraits<PlatformTag::Unix>;

  /// The file descriptor type on a POSIX system.
  using raw_fd = Traits::raw_fd;

  /// The type used by system calls dealing with flags.
  using flag_t = decltype(O_RDONLY);

  /// Duplicates the file descriptor \p Handle and wraps it into the RAII
  /// object.
  ///
  /// \see dup(2)
  [[nodiscard]] static fd dup(fd& Handle);

  /// Creates an empty file descriptor that does not wrap anything.
  fd() noexcept = default;

  /// Wrap the raw platform resource handle into the RAII object.
  fd(raw_fd Value) noexcept;

  /// Returns the \b raw file descriptor for the standard C I/O object.
  [[nodiscard]] static raw_fd fileno(std::FILE* File);

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

  /// Shortcut function that sets \p O_NONBLOCK on a file.
  static void setNonBlocking(raw_fd FD) noexcept;
  /// Shortcut function that removes \p O_NONBLOCK from a file.
  static void setBlocking(raw_fd FD) noexcept;

  /// Shortcut function that sets \p O_NONBLOCK and \p FD_CLOEXEC on a file.
  /// This results in the file set to not block when reading from, and to not
  /// be inherited by child processes in a \p fork() - \p exec() situation.
  static void setNonBlockingCloseOnExec(raw_fd FD) noexcept;
};

static_assert(sizeof(Handle) == sizeof(fd),
              "Handle implementation MUST be non-polymorphic!");

} // namespace monomux::system::unix
