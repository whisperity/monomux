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
#include <memory>
#include <string>
#include <utility>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/BufferedChannel.hpp"
#include "monomux/system/Handle.hpp"

namespace monomux::system
{

/// A pipe is a one-way communication channel between a reading and a writing
/// end.
///
/// Data written to the pipe's write end is buffered by the kernel and
/// can be read on the read end.
class Pipe : public BufferedChannel
{
public:
  /// The mode with which the \p Pipe is opened.
  enum Mode
  {
    /// Sentinel.
    None = 0,

    /// Open the read end of the pipe.
    Read = 1,

    /// Open the write end of the pipe.
    Write = 2
  };

  /// Wrapper for the return type of \p create() which creates an anonymous
  /// pipe that only exists as file descriptors, but not named entities.
  struct AnonymousPipe
  {
    AnonymousPipe(std::unique_ptr<Pipe>&& Read, std::unique_ptr<Pipe>&& Write)
      : Read(std::move(Read)), Write(std::move(Write))
    {}

    /// \returns the pipe for the read end.
    Pipe* getRead() const noexcept { return Read.get(); }
    /// \returns the pipe for the write end.
    Pipe* getWrite() const noexcept { return Write.get(); }

    /// Take ownership for the read end of the pipe, and close the write end.
    std::unique_ptr<Pipe> takeRead();
    /// Take ownership for the write end of the pipe, and close the read end.
    std::unique_ptr<Pipe> takeWrite();

  private:
    friend class Pipe;

    std::unique_ptr<Pipe> Read;
    std::unique_ptr<Pipe> Write;
  };

  ~Pipe() noexcept override;
  Pipe(Pipe&&) noexcept = default;
  Pipe& operator=(Pipe&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

  std::size_t optimalReadSize() const noexcept override;
  std::size_t optimalWriteSize() const noexcept override;

protected:
  Pipe(Handle FD,
       std::string Identifier,
       bool NeedsCleanup,
       Mode OpenMode,
       std::size_t BufferSize);

  UniqueScalar<Mode, None> OpenedAs;
  UniqueScalar<bool, false> Weak;
};

} // namespace monomux::system
