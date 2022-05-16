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
#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Channel.hpp"

namespace monomux
{

namespace detail
{

/// A ballooning buffer for read/write requests. In some implementations, the
/// actual low-level read operation might consume (and thus make unavailable)
/// and return more data than the user requested. This overflow is stored in
/// this buffer, and served first at subsequent read requests. When writing,
/// the buffer can store physically unsent data until it is possible to send
/// everything.
class BufferedChannelBuffer;

} // namespace detail

/// A special implementation of \p Channel that performs locally (userspace)
/// buffered reads and writes.
///
/// Common implementations of low-level OS primitives do not offer guarantees
/// as to whether a read or write operation of a particular size will actually
/// read/write that many data. In some cases, reading \p N bytes might consume
/// a larger amount from the kernel-backed data structure, in which case the
/// tail end is dropped.
class BufferedChannel : public Channel
{
  using OpaqueBufferType = detail::BufferedChannelBuffer;

public:
  /// The initial size of the buffers that are allocated for a
  /// \p BufferedChannel.
  static constexpr std::size_t BufferSize = 1ULL << 14; // 16 KiB

  /// Thrown if the \p Buffer of a \p BufferedChannel exceeds a (reasonable)
  /// size limit.
  class OverflowError : public std::runtime_error
  {
    static std::string craftErrorMessage(const std::string& Identifier,
                                         std::size_t Size);

    const BufferedChannel& Channel;
    bool Read;
    bool Write;

  public:
    OverflowError(const BufferedChannel& Channel,
                  const std::string& Identifier,
                  std::size_t Size,
                  bool Read,
                  bool Write)
      : std::runtime_error(craftErrorMessage(Identifier, Size)),
        Channel(Channel), Read(Read), Write(Write)
    {}

    const char* what() const noexcept override
    {
      return std::runtime_error::what();
    }

    const BufferedChannel& channel() const noexcept { return Channel; }
    raw_fd fd() const noexcept { return Channel.raw(); }
    bool readOverflow() const noexcept { return Read; }
    bool writeOverflow() const noexcept { return Write; }
  };

  BufferedChannel() = delete;
  ~BufferedChannel() override;

  /// Reads and consumes data from the channel and returns at \b maximum
  /// \p Bytes of it.
  ///
  /// This function \e buffers: if there is data already avaialble locally,
  /// that will be returned first. If the underlying implementation loads more
  /// than requested, the tail end will also be saved.
  ///
  /// Sufficiently sized requests do not interact with the buffer.
  ///
  /// \throws buffer_overflow If the buffer is interacted with and exceeds the
  /// limit \p BufferSizeMax, the command throws. \p BufferSizeMax is a soft
  /// limit enforced by this class, not the underlying structure. The read data
  /// is \e NOT lost, but stored into the buffer, however, care must be taken
  /// so that system resources are not exhausted.
  ///
  /// \see load
  std::string read(std::size_t Bytes);

  /// Writes the contents of \p Data into the channel.
  ///
  /// This function \e buffers: if thers is data that had been put into the
  /// underlying buffer but not sent to the underlying primitive yet, the
  /// buffer will be cleared first, in order. \p Data is not discarded, however,
  /// if the underlying implementation rejected the sending of the contents of
  /// \p Data, it will be added to the buffer.
  ///
  /// \returns the number of bytes of \p Data written to the channel.
  ///
  /// \throws buffer_overflow If the buffer is interacted with and exceeds the
  /// limit \p BufferSizeMax, the command throws. \p BufferSizeMax is a soft
  /// limit enforced by this class, not the underlying structure. The unwritten
  /// data is \e NOT lost, but stored into the buffer, however, care must be
  /// taken so that system resources are not exhausted.
  std::size_t write(std::string_view Data);

  /// Reads at \b least \p Bytes bytes from the underlying implementation,
  /// consuming it, and unconditionally placing it into the locally held buffer.
  ///
  /// \returns the number of writes read and placed.
  ///
  /// \throws buffer_overflow If the buffer is interacted with and exceeds the
  /// limit \p BufferSizeMax, the command throws. \p BufferSizeMax is a soft
  /// limit enforced by this class, not the underlying structure. The read data
  /// is \e NOT lost, but stored into the buffer, however, care must be taken
  /// so that system resources are not exhausted.
  ///
  /// \see read
  std::size_t load(std::size_t Bytes);

  /// Performs \p write() only on the contents of the already established
  /// buffer. Not all data might be actually written out.
  ///
  /// \returns the number of bytes successfully written.
  ///
  /// This function is incapable of \e increasing the size of the buffer, and
  /// thus will not throw \p buffer_overflow.
  std::size_t flushWrites();

  /// \returns whether there are buffered data read but not yet consumed.
  bool hasBufferedRead() const noexcept;
  /// \returns whether there are buffered data written but not yet flushed.
  bool hasBufferedWrite() const noexcept;
  /// \returns the number of bytes already read, but not yet consumed.
  std::size_t readInBuffer() const noexcept;
  /// \returns the number of bytes already written but not yet flushed.
  std::size_t writeInBuffer() const noexcept;

protected:
  UniqueScalar<OpaqueBufferType*, nullptr> Read;
  UniqueScalar<OpaqueBufferType*, nullptr> Write;

  /// Creates the buffering structure for the object.
  /// \param ReadBufferSize If non-zero, the size of the read buffer. If zero,
  /// a read buffer will not be created.
  /// \param WriteBufferSize If non-zero, the size of the write buffer. If zero,
  /// a write buffer will not be created.
  BufferedChannel(fd Handle,
                  std::string Identifier,
                  bool NeedsCleanup,
                  std::size_t ReadBufferSize = BufferSize,
                  std::size_t WriteBufferSize = BufferSize);
  BufferedChannel(BufferedChannel&&) noexcept = default;
  BufferedChannel& operator=(BufferedChannel&&) noexcept = default;

  /// \returns the size of low-level single read operations that are in some
  /// sense "optimal" for the underlying implementation.
  virtual std::size_t optimalReadSize() const noexcept { return BufferSize; }
  /// \returns the size of low-level single write operations that are in some
  /// sense "optimal" for the underlying implementation.
  virtual std::size_t optimalWriteSize() const noexcept { return BufferSize; }
};

using buffer_overflow = BufferedChannel::OverflowError;

} // namespace monomux
