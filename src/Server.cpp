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
#include "Server.hpp"
#include "Environment.hpp"

#include <utility>

namespace monomux
{

bool Server::currentProcessMarkedAsServer()
{
  return getEnv("MONOMUX_SERVER") == "YES";
}

std::string Server::getServerSocketPath()
{
  // TODO: Handle XDG_RUNTIME_DIR, etc.
  return "monomux.server.sock";
}

Server::Server(Socket&& Sock) : Sock(std::move(Sock)) {}

} // namespace monomux
