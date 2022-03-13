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
#include "fd.hpp"

#include <string>

#include <fcntl.h>

namespace monomux
{

/// This class is used to create OR open a pipe (a file on the disk), which
/// allows either reading OR writing (but not both!) data. Data written to the
/// pipe is buffered by the kernel and appears on the read end of the pipe.
class Pipe /* TODO: : public CommunicationChannel */
{
public:
  /// The mode with which the \p Pipe is opened.
  enum Mode
  {
    /// Open the read end of the pipe.
    Read = O_RDONLY,
    /// Open the write end of the pipe.
    Write = O_WRONLY
  };

  /// Creates a new named pipe (\p FIFO)  which will be owned by the current
  /// instance, and cleaned up on exit.
  ///
  /// This call opens the \p Pipe in \p Write mode.
  ///
  /// If \p InheritInChild is true, the pipe will be flagged to be inherited
  /// by a potential child process.
  static Pipe create(std::string Path, bool InheritInChild = false);

  /// Opens a connection to the named pipe (\p FIFO) existing at \p Path. The
  /// connection will be cleaned up during destruction, but no attempts will be
  /// made to destroy the named entity itself.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  static Pipe
  open(std::string Path, Mode OpenMode = Read, bool InheritInChild = false);

  /// Wraps the already existing file descriptor \p FD as a \p Pipe. The new
  /// instance will take ownership and close the resource at the end of its
  /// life.
  static Pipe wrap(fd&& FD, Mode OpenMode = Read);

  /// Closes the connection, and if the \p Pipe was created by this instance,
  /// clears it up.
  ~Pipe() noexcept;

  Pipe(Pipe&&) noexcept;
  Pipe& operator=(Pipe&&) noexcept;

  /// Returns the raw file descriptor for the underlying resource.
  raw_fd raw() const noexcept { return Handle.get(); }

  /// Returns the associated path with the pipe, if it was a named pipe.
  const std::string& getPath() const noexcept { return Path; }

  /// Sets the open pipe to be \e non-blocking. \p Read operations will
  /// immediately return without data, and \p Write will fail if the pipe is
  /// full of unread contents.
  void setBlocking();
  /// Sets the open pipe to be \b blocking. \p Read operations will wait until
  /// data is available, and \p Write into a full pipe will wait until a reader
  /// consumed enough data.
  void setNonblocking();

  bool isBlocking() const noexcept { return !Nonblock; }
  bool isNonblocking() const noexcept { return Nonblock; }

  /// Directly read and consume at most \p Bytes of data from the pipe, if it
  /// was opened in \p Read mode.
  std::string read(std::size_t Bytes);

  /// Directly read and consume at most \p Bytes of data from the given file
  /// descriptor \p FD.
  ///
  /// \param Success If not \p nullptr, and the read encounters an error, will
  /// be set to \p false.
  static std::string read(fd& FD, std::size_t Bytes, bool* Success = nullptr);

  /// Returns whether the instance believes that the underlying resource is
  /// still open. This is an "a posteriori" method. Certain accesses WILL set
  /// the flag to be not open anymore.
  bool believeConnectionOpen() const noexcept { return Open; }

  /// Write \p Data into the pipe, if it is was opened in \p Write mode.
  void write(std::string_view Data);

  /// Write \p Data into the file descriptor \p FD.
  ///
  /// \param Success If not \p nullptr, and the write encounters an error, will
  /// be set to \p false.
  static void write(fd& FD, std::string_view Data, bool* Success = nullptr);

private:
  Pipe() = default;

  fd Handle;
  Mode OpenedAs;
  std::string Path;
  bool Owning;
  bool CleanupPossible;
  bool Open = true;
  bool Nonblock = false;
};

} // namespace monomux
