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

#include "system/Socket.hpp"
#include "system/fd.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>

namespace monomux
{

class EPoll;

/// The monomux server is responsible for creating child processes of sessions.
/// Clients communicate with a \p Server instance to obtain information about
/// a session and to initiate attachment procedures.
class Server
{
public:
  static std::string getServerSocketPath();

  /// Create a new server that will listen on the associated socket.
  Server(Socket&& Sock);

  ~Server();

  /// Start actively listening and handling connections.
  ///
  /// \note This is a blocking call!
  int listen();

private:
  Socket Sock;
  std::map<std::uint16_t, std::function<void(Socket&, std::string)>> Dispatch;
  std::map<raw_fd, std::unique_ptr<Socket>> ClientSockets;

  std::atomic_bool TerminateListenLoop = ATOMIC_VAR_INIT(false);
  EPoll* Poll = nullptr;

  void acceptCallback(Socket& Client);
  void readCallback(Socket& Client);
  void exitCallback(Socket& Client);

private:
  void setUpDispatch();

#define DISPATCH(KIND, FUNCTION_NAME)                                          \
  void FUNCTION_NAME(Socket& Client, std::string RawMessage);
#include "Server.Dispatch.ipp"
};

} // namespace monomux
