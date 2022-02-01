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
#include "Socket.hpp"
#include "CheckedPOSIX.hpp"
#include "POD.hpp"

#include <iostream>

#include <sys/un.h>
#include <unistd.h>

namespace monomux
{

  Socket Socket::create(std::string Path) {
    fd Handle = CheckedPOSIXThrow([] {
          return ::socket(AF_UNIX, SOCK_STREAM, 0);
        }, "Error creating socket '" + Path + "'", -1);

    // TODO: Mask?

    POD<struct ::sockaddr_un> SocketAddr;
    SocketAddr->sun_family = AF_UNIX;
    std::strncpy(
      SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

    CheckedPOSIXThrow(
      [Handle, &SocketAddr] {
        return ::bind(Handle,
                      reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                      sizeof(SocketAddr));
      },
      "Failed to bind '" + Path + "'",
      -1);

    Socket S;
    S.Owning = true;
    S.Handle = Handle;
    S.Path = std::move(Path);
    return S;
  }

  Socket Socket::open(std::string Path) {
    fd Handle = CheckedPOSIXThrow([] {
          return ::socket(AF_UNIX, SOCK_STREAM, 0);
        }, "Error creating temporary client socket", -1);

    POD<struct ::sockaddr_un> SocketAddr;
    SocketAddr->sun_family = AF_UNIX;
    std::strncpy(
      SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

    CheckedPOSIXThrow(
      [Handle, &SocketAddr] {
        return ::connect(Handle,
                      reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                      sizeof(SocketAddr));
      },
      "Failed to connect to '" + Path + "'",
      -1);

    Socket S;
    S.Owning = false;
    S.Handle = Handle;
    S.Path = std::move(Path);
    return S;
  }

  Socket::~Socket() {
    if (Owning) {
      auto RemoveResult = CheckedPOSIX([this] {
            return ::unlink(Path.c_str());
          }, -1);
      if (!RemoveResult) {
        std::cerr << "Failed to remove '" << Path
                  << "' when closing the socket.\n";
        std::cerr << std::strerror(RemoveResult.getError().value())
                  << std::endl;
      }
    }

    CheckedPOSIX([this] { return ::close(Handle); }, -1);
  }

} // namespace monomux
