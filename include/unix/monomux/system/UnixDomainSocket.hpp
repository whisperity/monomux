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

#include "monomux/system/UnixSocket.hpp"

namespace monomux::system::unix
{

/// This class wraps a Unix domain socket (appearing to applications as a named
/// file in the filesystem) and allows reading or writing to the socket.
///
/// This implementation uses \p SOCK_STREAM.
class DomainSocket : public Socket
{
public:
  /// Creates a new \p DomainSocket which will be owned by the current
  /// instance, and removed on exit. Such sockets can be used to await
  /// connections and implement server-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see bind(2)
  static DomainSocket create(std::string Path, bool InheritInChild = false);

  /// Opens a connection to the socket existing in the file system at \p Path.
  /// The connection will be cleaned up during destruction, but the file entity
  /// is left intact. A low-level connection is initiated through the socket.
  /// Such sockets can be used to implement clients-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see connect(2)
  static DomainSocket connect(std::string Path, bool InheritInChild = false);

  /// Wraps an already existing file descriptor, \p FD as a socket.
  /// Ownership of the resource itself is taken by the \p Socket instance and
  /// the file will be closed during destruction, but no additional cleanup
  /// may take place.
  ///
  /// \param Identifier An identifier to assign to the \p Socket. If empty, a
  /// default value will be created.
  ///
  /// \note This method does \b NOT verify whether the wrapped file descriptor
  /// is indeed a socket, and assumes it is set up (either in server, or client
  /// mode) already.
  static DomainSocket wrap(fd&& FD, std::string Identifier);

  ~DomainSocket() noexcept override;
  DomainSocket(DomainSocket&&) noexcept = default;
  DomainSocket& operator=(DomainSocket&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

protected:
  DomainSocket(Handle FD,
               std::string Identifier,
               bool NeedsCleanup,
               bool Owning);

  /// \see accept(2)
  std::unique_ptr<system::Socket> acceptImpl(std::error_code* Error,
                                             bool* Recoverable) override;
};

} // namespace monomux::system::unix
