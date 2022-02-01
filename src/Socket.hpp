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

#include <sys/socket.h>

namespace monomux
{

/// This class is used to create OR open a Unix domain socket (a file on the
/// disk) and allow reading and writing from it.
class Socket
{
public:
  using fd = decltype(::socket(0, 0, 0));

  /// Creates a new \p Socket which will be owned by the current instance, and
  /// cleaned up on exit. Such sockets can be used to implement servers.
  static Socket create(std::string Path);

  /// Opens a connection to the socket existing and bound at \p Path. The
  /// connection will be cleaned up during destruction, but no attempts will be
  /// made to destroy the socket itself.
  static Socket open(std::string Path);

  Socket() = default;

  /// If the socket was created by this instance, clear it up.
  ~Socket();

  Socket(Socket&&) = default;

private:
  std::string Path;
  bool Owning;
  fd Handle;
};

} // namespace monomux
