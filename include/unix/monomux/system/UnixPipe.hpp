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

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Pipe.hpp"
#include "monomux/system/fd.hpp"

namespace monomux::system::unix
{

/// This class wraps a nameless pipe or a Unix named pipe (\e FIFO) appearing as
/// a file in the filesystem, and allows reading or writing (but noth both!) to
/// it.
class Pipe : public system::Pipe
{
public:
  /// Creates a new named pipe (\e FIFO) which will be owned by the current
  /// instance, and cleaned up on exit.
  ///
  /// This call opens the \p Pipe in \p Write mode.
  ///
  /// If \p InheritInChild is true, the pipe will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see mkfifo(3)
  static Pipe create(std::string Path, bool InheritInChild = false);

  /// Creates a new unnamed pipe which will be owned by the current instance,
  /// and cleaned up on exit.
  static AnonymousPipe create(bool InheritInChild = false);

  /// Opens a connection to the named pipe (\e FIFO) existing at \p Path. The
  /// connection will be cleaned up during destruction, but no attempts will be
  /// made to destroy the named entity itself.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see open(2)
  static Pipe
  open(std::string Path, Mode OpenMode = Read, bool InheritInChild = false);

  /// Wraps the already existing file descriptor \p FD as a \p Pipe. The new
  /// instance will take ownership and close the resource at the end of its
  /// life.
  ///
  /// \param Identifier An identifier to assign to the \p Pipe. If empty, a
  /// default value will be created.
  ///
  /// \note This method does \b NOT verify whether the wrapped file descriptor
  /// is indeed a pipe, and assumes it is set up appropriately.
  static Pipe wrap(fd&& FD, Mode OpenMode = Read, std::string Identifier = "");

  /// Wraps the already existing file descriptor \p FD as a \p Pipe. The new
  /// instance will \b NOT \b TAKE ownership or close the resource at the end
  /// of its life.
  ///
  /// \param Identifier An identifier to assign to the \p Pipe. If empty, a
  /// default value will be created.
  ///
  /// \note This method does \b NOT verify whether the wrapped file descriptor
  /// is indeed a pipe, and assumes it is set up appropriately.
  static Pipe
  weakWrap(fd::raw_fd FD, Mode OpenMode = Read, std::string Identifier = "");

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

  ~Pipe() noexcept override;
  Pipe(Pipe&&) noexcept = default;
  Pipe& operator=(Pipe&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

  std::size_t optimalReadSize() const noexcept override;
  std::size_t optimalWriteSize() const noexcept override;

protected:
  Pipe(fd::raw_fd FD, std::string Identifier, bool NeedsCleanup, Mode OpenMode);

  std::string readImpl(std::size_t Bytes, bool& Continue) override;
  std::size_t writeImpl(std::string_view Buffer, bool& Continue) override;

private:
  UniqueScalar<bool, false> Nonblock;
};

} // namespace monomux::system::unix
