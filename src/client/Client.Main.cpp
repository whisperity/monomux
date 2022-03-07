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
#include "Client.Main.hpp"
#include "Client.hpp"
#include "server/Server.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace monomux
{

namespace client
{

std::optional<Client> connect(const Options& Opts, bool Block)
{
  auto C = Client::create(Server::getServerSocketPath());
  if (!Block)
    return C;

  while (!C)
  {
    std::clog << "DEBUG: Trying to connect to server again..." << std::endl;
    C = Client::create(Server::getServerSocketPath());
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::clog << "DEBUG: Connection established!" << std::endl;
  return C;
}

int main(Options& Opts)
{
  // For the convenience of auto-starting a server if none exists, the creation
  // of the Client itself is placed into the global entry point.
  assert(Opts.Connection.has_value() && "main() should have created a client.");

  while (!Opts.Connection->handshake())
  {
    std::clog << "DEBUG: Trying to authenticate with server again..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return EXIT_SUCCESS;
}

} // namespace client

} // namespace monomux
