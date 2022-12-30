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
#include <string_view>
#include <vector>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Handle.hpp"

namespace monomux::system
{

/// Wraps a system resource used for communication. This is a very low-level
/// interface encapsulating the necessary system calls and transmission logic.
class Channel
{
public:
  Channel() = delete;

  /// \returns the raw, unmanaged file descriptor for the underlying resource.
  [[nodiscard]] Handle::Raw raw() const noexcept { return FD.get(); }

  /// Steal the \p Handle file descriptor from the current communication channel
  /// marking it failed and preventing the cleanup of the resource.
  [[nodiscard]] Handle release() &&;

  /// \returns the user-friendly identifier of the communication channel. This
  /// might be empty, a transient label, or sometimes a path on the filesystem.
  [[nodiscard]] const std::string& identifier() const noexcept
  {
    return Identifier;
  }

  /// \returns whether an operation failed and indicated that the underlying
  /// resource had broken.
  [[nodiscard]] bool failed() const noexcept { return !FD.has() || Failed; }

  /// Read at maximum \p Bytes bytes of data from the communication channel.
  ///
  /// \warning Depending on the implementation of the OS primitive and its
  /// state, this operation \b MAY block, or \b MAY return less than exactly
  /// \p Bytes of data, or even fail completely, if the channel does not support
  /// reading. It is also possible that the underlying implementation reads
  /// \b more than \p Bytes of data, in which case the tail end is discarded.
  [[nodiscard]] std::string read(std::size_t Bytes);

  /// Write the contents of \p Buffer into the communication channel.
  ///
  /// \warning Depending on the implementation of the OS primitive and its
  /// state, this operation \b MAY block, or \b MAY write less than the entire
  /// buffer. In some cases, it also \b MAY fail completely, if the channel does
  /// not support writing.
  ///
  /// \returns the number of bytes written, as reported by the operating system.
  std::size_t write(std::string_view Buffer);

  virtual ~Channel() noexcept = default;

protected:
  Channel(Handle FD, std::string Identifier, bool NeedsCleanup);
  Channel(Channel&&) noexcept = default;
  Channel& operator=(Channel&&) noexcept = default;

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

  [[nodiscard]] bool needsCleanup() const noexcept { return EntityCleanup; }
  void setFailed() noexcept { Failed = true; }

  Handle FD;
  std::string Identifier;

private:
  UniqueScalar<bool, false> EntityCleanup;
  UniqueScalar<bool, false> Failed;
};

} // namespace monomux::system
