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
#include "monomux/control/Message.hpp"
#include "monomux/control/PascalString.hpp"

#include "monomux/client/ControlClient.hpp"

namespace monomux::client
{

ControlClient::ControlClient(Client& C) : BackingClient(C) {}

ControlClient::ControlClient(Client& C, std::string Session)
  : BackingClient(C), SessionName(std::move(Session))
{
  BackingClient.requestAttach(SessionName);
}

void ControlClient::requestDetachLatestClient()
{
  using namespace monomux::message;
  if (!BackingClient.attached())
    return;

  auto X = BackingClient.inhibitControlResponse();
  sendMessage(BackingClient.getControlSocket(),
              request::Detach{request::Detach::Latest});
  receiveMessage<response::Detach>(BackingClient.getControlSocket());
}

void ControlClient::requestDetachAllClients()
{
  using namespace monomux::message;
  if (!BackingClient.attached())
    return;

  auto X = BackingClient.inhibitControlResponse();
  sendMessage(BackingClient.getControlSocket(),
              request::Detach{request::Detach::All});
  receiveMessage<response::Detach>(BackingClient.getControlSocket());
}

std::string ControlClient::requestStatistics()
{
  using namespace monomux::message;

  auto X = BackingClient.inhibitControlResponse();
  sendMessage(BackingClient.getControlSocket(), request::Statistics{});
  auto Response =
    receiveMessage<response::Statistics>(BackingClient.getControlSocket());

  if (!Response)
    throw std::runtime_error{"Failed to receive a valid response!"};
  return std::move(Response)->Contents;
}

} // namespace monomux::client
