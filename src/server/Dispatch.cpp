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
#include "control/Messaging.hpp"
#include "system/Process.hpp"

using namespace monomux::message;

namespace monomux
{
namespace server
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
#include "Dispatch.ipp"
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME)                                                          \
  void Server::NAME(ClientData& Client, std::string_view Message)

#define MSG(TYPE)                                                              \
  std::optional<TYPE> Msg = TYPE::decode(Message);                             \
  if (!Msg)                                                                    \
    return;

HANDLER(requestClientID)
{
  MSG(request::ClientID);
  std::cout << "SERVER: Client #" << Client.id() << ": Request Client ID"
            << std::endl;

  response::ClientID Resp;
  Resp.Client.ID = Client.id();
  Resp.Client.Nonce = Client.makeNewNonce();

  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(requestDataSocket)
{
  MSG(request::DataSocket);
  response::DataSocket Resp;
  Resp.Success = false;

  // In this function, Client is the message sender, so the connection that
  // wants to become the data socket.

  std::cout << "Server: Client #" << Client.id()
            << ": Associate as Data Socket for " << Msg->Client.ID << std::endl;

  auto MainIt = Clients.find(Msg->Client.ID);
  if (MainIt == Clients.end())
  {
    sendMessage(Client.getControlSocket(), Resp);
    return;
  }

  ClientData& MainClient = *MainIt->second;
  if (MainClient.getDataSocket() != nullptr)
  {
    sendMessage(Client.getControlSocket(), Resp);
    return;
  }
  if (MainClient.consumeNonce() != Msg->Client.Nonce)
  {
    sendMessage(Client.getControlSocket(), Resp);
    return;
  }

  turnClientIntoDataOfOtherClient(MainClient, Client);
  assert(MainClient.getDataSocket() &&
         "Turnover should have subjugated client!");
  Resp.Success = true;
  sendMessage(*MainClient.getDataSocket(), Resp);
}

HANDLER(requestSessionList)
{
  MSG(request::SessionList);
  response::SessionList Resp;

  for (const auto& SessionElem : Sessions)
  {
    monomux::message::SessionData TransmitData;
    TransmitData.Name = SessionElem.first;
    TransmitData.Created = std::chrono::system_clock::to_time_t(SessionElem.second->whenCreated());

    Resp.Sessions.emplace_back(std::move(TransmitData));
  }

  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(requestMakeSession)
{
  MSG(request::MakeSession);
  std::cout << "Spawn: " << Msg->SpawnOpts.Program << std::endl;

  Process::SpawnOptions SOpts;
  SOpts.CreatePTY = true;
  SOpts.Program = Msg->SpawnOpts.Program;

  std::clog << "DEBUG: Spawning '" << SOpts.Program << "'..." << std::endl;
  Process P = Process::spawn(SOpts);

  // FIXME: Make this with no collision! Currently the child throws if a client
  // disconnects and connects again.
  std::string SessionName = "client-";
  SessionName.append(std::to_string(Client.id()));
  SessionName.push_back(':');
  SessionName.append(SOpts.Program);
  std::string SessionName2 = SessionName;

  auto S = std::make_unique<SessionData>(std::move(SessionName));
  S->setProcess(std::move(P));

  auto InsertResult =
    Sessions.try_emplace(std::move(SessionName2), std::move(S));

  createCallback(*InsertResult.first->second);

  // TODO: Response?
}

#undef HANDLER

} // namespace server
} // namespace monomux
