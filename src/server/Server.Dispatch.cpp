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

#include "control/Message.hpp"
#include "control/SocketMessaging.hpp"
#include "system/Process.hpp"

namespace monomux
{

void Server::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME)                                                           \
  std::bind(std::mem_fn(&Server::NAME),                                        \
            this,                                                              \
            std::placeholders::_1,                                             \
            std::placeholders::_2)
#define DISPATCH(K, FUNCTION) Dispatch.try_emplace(KIND(K), MEMBER(FUNCTION));
#include "Server.Dispatch.ipp"
#undef DISPATCH
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME)                                                          \
  void Server::NAME(ClientData& Client, std::string_view Message)

HANDLER(clientID)
{
  auto Msg = request::ClientID::decode(Message);
  if (!Msg)
    return;

  std::cout << "SERVER: Client #" << Client.id() << ": Request Client ID"
            << std::endl;

  response::ClientID Resp;
  Resp.ID = Client.id();
  Resp.Nonce = Client.makeNewNonce();

  writeMessage(Client.getControlSocket(), std::move(Resp));
}

HANDLER(spawnProcess)
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;

  auto Msg = request::SpawnProcess::decode(Message);
  if (!Msg)
    return;

  std::cout << "Spawn: " << Msg->ProcessName << std::endl;

  Process::SpawnOptions SOpts;
  SOpts.Program = Msg->ProcessName;
  SOpts.CreatePTY = true;

  Process P = Process::spawn(SOpts);
}

#undef HANDLER

} // namespace monomux
