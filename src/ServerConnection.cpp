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
#include "ServerConnection.hpp"
#include "Message.hpp"
#include "SocketMessaging.hpp"

#include <iostream>
#include <utility>

namespace monomux
{

std::optional<ServerConnection> ServerConnection::create(std::string SocketPath)
{
  try
  {
    ServerConnection Conn{Socket::open(SocketPath)};
    return Conn;
  }
  catch (const std::system_error& Err)
  {
    std::cerr << "When creating ServerConnection with '" << SocketPath
              << "': " << Err.what() << std::endl;
  }
  return std::nullopt;
}

ServerConnection::ServerConnection(Socket&& ControlSock)
  : ControlSocket(std::move(ControlSock))
{
  writeMessage(ControlSocket, request::ClientID{});
  std::string Data = ControlSocket.read(128);
  std::cout << "Received data: " << Data << "\n";

  // If the control socket is established, establish another connection for the
  // data socket.
}

void ServerConnection::requestSpawnProcess(const Process::SpawnOptions& Opts)
{
  request::SpawnProcess Msg;
  Msg.ProcessName = Opts.Program;

  writeMessage(ControlSocket, std::move(Msg));
}

} // namespace monomux
