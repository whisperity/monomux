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
#include "adt/unique_scalar.hpp"
#include "fd.hpp"

#include <string>
#include <vector>

namespace monomux
{

/// Wraps a system resource used for communication. This is a very low-level
/// interface encapsulating the necessary system calls and transmission logic.
class CommunicationChannel
{
public:
  CommunicationChannel() = delete;

  /// \returns the raw, unmanaged file descriptor for the underlying resource.
  raw_fd raw() const noexcept { return Handle.get(); }

  /// \returns the user-friendly identifier of the communication channel. This
  /// might be empty, a transient label, or sometimes a path on the filesystem.
  const std::string& identifier() const noexcept { return Identifier; }

  /// \returns whether an operation failed and indicated that the underlying
  /// resource had broken.
  bool failed() const noexcept { return !Handle.has() || Failed; }

  /// Read at maximum \p Bytes bytes of data from the communication channel.
  ///
  /// Depending on the implementation of the OS primitive and its state, this
  /// operation \b MAY block, or \b MAY return less than exactly \p Bytes of
  /// data, or even fail completely, if the channel does not support reading.
  std::string read(std::size_t Bytes);

  /// Reads at maximum \p Bytes bytes of data from the communication channel,
  /// bypassing any buffering logic.
  std::string readImmediate(std::size_t Bytes);

  /// Write the contents of \p Buffer into the communication channel.
  ///
  /// Depending on the implementation of the OS primitive and its state, this
  /// operation \b MAY block, or \b MAY write less than the entire buffer. In
  /// some cases, it also \b MAY fail completely, if the channel does not
  /// support writing.
  ///
  /// \returns the number of bytes written, as reported by the operating system.
  std::size_t write(std::string_view Buffer);

  /// Writes the contents of \p Buffer into the communcation channel, bypassing
  /// any additional buffering logic.
  std::size_t writeImmediate(std::string_view Buffer);

  virtual ~CommunicationChannel() noexcept = default;

protected:
  static constexpr std::size_t BufferSize = 1 << 14;

  CommunicationChannel(fd Handle, std::string Identifier, bool NeedsCleanup);
  CommunicationChannel(CommunicationChannel&&) noexcept;
  CommunicationChannel& operator=(CommunicationChannel&&) noexcept;

  /// Implemented by subclases to actually perform reading from the system.
  ///
  /// \param Continue Whether the read operation from the low-level resource
  /// might continue, because there is more data available.
  virtual std::string readImpl(std::size_t Bytes, bool& Continue) = 0;
  /// Implemented by subclases to actually perform writing to the system.
  ///
  /// \param Continue Whether the write operation to the low-level resource
  /// might continue, because there is more space available.
  virtual std::size_t writeImpl(std::string_view Buffer, bool& Continue) = 0;

  bool needsCleanup() const noexcept { return EntityCleanup; }
  void setFailed() noexcept { Failed = true; }

  fd Handle;
  std::string Identifier;
  /// A ballooning buffer for read requests. In some implementations, the
  /// actual low-level read operation might consume (and thus make unavailable)
  /// and return more data than the user requested. This overflow is stored in
  /// this buffer, and served first at subsequent read requests.
  std::vector<char> ReadBuffer;
  /// \see ReadBuffer
  std::vector<char> WriteBuffer;

private:
  unique_scalar<bool, false> EntityCleanup;
  unique_scalar<bool, false> Failed;
};

} // namespace monomux
