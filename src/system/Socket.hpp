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

namespace monomux
{

/// This class is used to create OR open a Unix domain socket (a file on the
/// disk) and allow reading and writing from it. This is a very low level
/// interface on top of the related system calls.
class Socket
{
public:
  /// Creates a new \p Socket which will be owned by the current instance, and
  /// cleaned up on exit. Such sockets can be used to implement servers.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  static Socket create(std::string Path, bool InheritInChild = false);

  /// Opens a connection to the socket existing and bound at \p Path. The
  /// connection will be cleaned up during destruction, but no attempts will be
  /// made to destroy the socket itself.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  static Socket open(std::string Path, bool InheritInChild = false);

  /// Wraps the already existing file descriptor \p FD as a socket. The new
  /// instance will take ownership and clear the resource up at the end of
  /// its life.
  static Socket wrap(fd&& FD);

  /// Closes the connection, and if the socket was created by this instance,
  /// clears it up.
  ~Socket() noexcept;

  Socket(Socket&&) noexcept;
  Socket& operator=(Socket&&) noexcept;

  /// Returns the raw file descriptor for the underlying resource.
  raw_fd raw() const noexcept { return Handle.get(); }

  /// Returns the associated path with the socket.
  const std::string& getPath() const noexcept { return Path; }

  /// Marks the socket as a listen socket via the \p listen() syscall.
  ///
  /// \note This operation is only valid if the instance owns the socket.
  void listen();

  /// Directly read and consume \p Bytes of data from the socket.
  std::string read(std::size_t Bytes);

  /// Returns whether the instance believes that the underlying resource is
  /// still open. This is an "a posteriori" method. Certain accesses WILL set
  /// the flag to be not open anymore.
  bool believeConnectionOpen() const noexcept { return Open; }

  /// Write \p Data to the socket.
  void write(std::string_view Data);

private:
  Socket() = default;

  fd Handle;
  std::string Path;
  bool Owning;
  bool CleanupPossible;
  bool Open = true;
  bool Listening = false;
};

} // namespace monomux
