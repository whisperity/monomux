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

#include "Process.hpp"
#include "Socket.hpp"

#include <optional>
#include <string>

namespace monomux
{

/// This class represents a connection to a running \p Server - or rather,
/// a wrapper over the communication channel that allows talking to the
/// server.
///
/// In networking terminology, this is effectively a client, but we reserve
/// that word for the "Monomux Client" which deals with attaching to a
/// process.
class ServerConnection
{
public:
  static std::optional<ServerConnection> create(std::string SocketPath);

  ServerConnection(Socket&& ConnSock);
  void requestSpawnProcess(const Process::SpawnOptions& Opts);

private:
  Socket Connection;
};

} // namespace monomux
