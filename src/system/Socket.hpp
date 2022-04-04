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
#include <optional>
#include <string>
#include <system_error>

#include "CommunicationChannel.hpp"
#include "fd.hpp"

namespace monomux
{

/// A socket is a two-way communication channel between a "client" and a
/// "server".
///
/// This class wraps a Unix domain socket (appearing to applications as a named
/// file in the filesystem) and allows reading or writing to the socket.
///
/// This implementation uses \p SOCK_STREAM, which is similar to TCP
/// connections: clients connect to the server socket and then a connection is
/// accepted, at which point the connection \e towards that new client becomes
/// its own \p Socket.
///
/// \note This object does \b NOT manage the set of connected clients or
/// anything related to such notion, only exposes the low-level system calls
/// facilitating socket behaviour.
///
/// \see socket(7)
class Socket : public CommunicationChannel
{
public:
  /// Creates a new \p Socket which will be owned by the current instance, and
  /// removed on exit. Such sockets can be used to await connections and
  /// implement server-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see bind(2)
  static Socket create(std::string Path, bool InheritInChild = false);

  /// Opens a connection to the socket existing in the file system at \p Path.
  /// The connection will be cleaned up during destruction, but the file entity
  /// is left intact. A low-level connection is initiated through the socket.
  /// Such sockets can be used to implement clients-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see connect(2)
  static Socket connect(std::string Path, bool InheritInChild = false);

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
  static Socket wrap(fd&& FD, std::string Identifier);

  /// Starts listening for incoming connection on the current socket by calling
  /// \p listen(). This is only valid if the current socket was created in full
  /// ownership mode, with the \p create() method.
  ///
  /// \see listen(2)
  void listen(std::size_t QueueSize);

  /// Accepts a new connection on the current serving socket. This operation
  /// \b MAY block. This call is only valid if the current socket was created in
  /// full ownership mode, and \p listen() had already been called for it.
  ///
  /// \param Error If non-null and the accepting of the client fails, the error
  /// code is returned in this parameter.
  /// \param Recoverable If non-null and the accepting of the client fails, but
  /// the low-level error is indicative of a potential to try again, will be set
  /// to \p true.
  ///
  /// \see accept(2)
  std::optional<Socket> accept(std::error_code* Error = nullptr,
                               bool* Recoverable = nullptr);

  ~Socket() noexcept override;
  Socket(Socket&&) noexcept;
  Socket& operator=(Socket&&) noexcept;

protected:
  Socket(fd Handle, std::string Identifier, bool NeedsCleanup);

  std::string readImpl(std::size_t Bytes, bool& Continue) override;
  std::size_t writeImpl(std::string_view Buffer, bool& Continue) override;

private:
  /// Whether the current instance is \e owning a socket, i.e. controlling it
  /// as a server.
  UniqueScalar<bool, false> Owning;
  UniqueScalar<bool, false> Listening;
};

} // namespace monomux
