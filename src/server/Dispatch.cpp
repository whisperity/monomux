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
#include "monomux/system/Environment.hpp"
#include "monomux/system/Process.hpp"

#include "monomux/server/Server.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("server/Dispatch")

using namespace monomux::message;

namespace monomux::server
{

void Server::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME) &Server::NAME
#define DISPATCH(K, FUNCTION) registerMessageHandler(KIND(K), MEMBER(FUNCTION));
#include "monomux/server/Dispatch.ipp"
#undef MEMBER
#undef KIND
}

/// Reschedules the overflown buffer identified by \p BO to the next iteration
/// of \p Poll.
template <typename T>
static std::size_t sendMessageAndRescheduleIfOverflow(EPoll& Poll,
                                                      BufferedChannel& Channel,
                                                      const T& Msg)
{
  try
  {
    return sendMessage(Channel, Msg);
  }
  catch (const buffer_overflow&)
  {
    Poll.schedule(Channel.raw(), /* Incoming =*/false, /* Outgoing =*/true);
    return 0;
  }
}

void Server::sendAcceptClient(ClientData& Client)
{
  sendMessageAndRescheduleIfOverflow(
    *Poll, Client.getControlSocket(), notification::Connection{{true}, {}});
}

void Server::sendRejectClient(ClientData& Client, std::string Reason)
{
  sendMessageAndRescheduleIfOverflow(
    *Poll,
    Client.getControlSocket(),
    notification::Connection{{false}, std::move(Reason)});
}

#define HANDLER(NAME)                                                          \
  void Server::NAME(                                                           \
    Server& Server, ClientData& Client, std::string_view Message)

#define MSG(TYPE)                                                              \
  std::optional<TYPE> Msg = TYPE::decode(Message);                             \
  if (!Msg)                                                                    \
    return;                                                                    \
  MONOMUX_TRACE_LOG(LOG(trace) << __PRETTY_FUNCTION__);

HANDLER(requestClientID)
{
  (void)Server;
  MSG(request::ClientID);
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

  auto MainIt = Server.Clients.find(Msg->Client.ID);
  if (MainIt == Server.Clients.end())
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

  Server.turnClientIntoDataOfOtherClient(MainClient, Client);
  assert(MainClient.getDataSocket() &&
         "Turnover should have subjugated client!");
  Resp.Success = true;
  sendMessage(*MainClient.getDataSocket(), Resp);
}

HANDLER(requestSessionList)
{
  MSG(request::SessionList);
  response::SessionList Resp;

  for (const auto& SessionElem : Server.Sessions)
  {
    monomux::message::SessionData TransmitData;
    TransmitData.Name = SessionElem.first;
    TransmitData.Created =
      std::chrono::system_clock::to_time_t(SessionElem.second->whenCreated());

    Resp.Sessions.emplace_back(std::move(TransmitData));
  }

  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(requestMakeSession)
{
  (void)Client;
  MSG(request::MakeSession);
  response::MakeSession Resp;
  Resp.Name = Msg->Name;
  Resp.Success = false;

  if (!Msg->Name.empty() && Server.getSession(Msg->Name))
  {
    LOG(debug) << "Session \"" << Msg->Name << "\" already exists";
    sendMessage(Client.getControlSocket(), Resp);
    return;
  }
  if (Msg->Name.empty())
  {
    // Generate a default session name, which will just be a numeric ID.
    std::size_t SessionNum = 1;
    while (Server.getSession(std::to_string(SessionNum)))
      ++SessionNum;
    Msg->Name = std::to_string(SessionNum);
  }

  LOG(info) << "Creating Session \"" << Msg->Name << "\"...";
  Resp.Name = Msg->Name;
  auto S = std::make_unique<SessionData>(std::move(Msg->Name));

  Process::SpawnOptions SOpts;
  SOpts.CreatePTY = true;
  SOpts.Program = std::move(Msg->SpawnOpts.Program);
  SOpts.Arguments = std::move(Msg->SpawnOpts.Arguments);
  for (std::pair<std::string, std::string>& EnvVar :
       Msg->SpawnOpts.SetEnvironment)
    SOpts.Environment.try_emplace(std::move(EnvVar.first),
                                  std::move(EnvVar.second));
  for (std::string& UnsetEnvVar : Msg->SpawnOpts.UnsetEnvironment)
    SOpts.Environment.try_emplace(std::move(UnsetEnvVar), std::nullopt);

  {
    // Inject the variables needed by the controlling client to detach from the
    // session.
    MonomuxSession MS;
    MS.SessionName = Resp.Name;
    MS.Socket = SocketPath::absolutise(Server.Sock.identifier());

    for (std::pair<std::string, std::string> BuiltinEnvVar : MS.createEnvVars())
      SOpts.Environment[std::move(BuiltinEnvVar.first)] =
        std::move(BuiltinEnvVar.second);
  }

  // TODO: How to detect process creation failing?
  Process P = Process::spawn(SOpts);
  S->setProcess(std::move(P));

  auto InsertResult = Server.Sessions.try_emplace(Resp.Name, std::move(S));
  Server.createCallback(*InsertResult.first->second);

  Resp.Success = true;
  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(requestAttach)
{
  MSG(request::Attach);
  response::Attach Resp;
  Resp.Success = false;

  SessionData* S = Server.getSession(Msg->Name);
  if (!S)
  {
    sendMessage(Client.getControlSocket(), Resp);
    return;
  }

  Server.clientAttachedCallback(Client, *S);
  Resp.Success = true;
  Resp.Session.Name = S->name();
  Resp.Session.Created = std::chrono::system_clock::to_time_t(S->whenCreated());
  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(requestDetach)
{
  using namespace monomux::message::request;
  MSG(request::Detach);
  response::Detach Resp;

  SessionData* S = Client.getAttachedSession();
  if (!S)
    return;

  std::vector<ClientData*> ClientsToDetach;

  switch (Msg->Mode)
  {
    case Detach::Latest:
      if (ClientData* C = S->getLatestClient())
        ClientsToDetach.emplace_back(C);
      break;
    case Detach::All:
      ClientsToDetach = S->getAttachedClients();
      break;
  }

  for (ClientData* C : ClientsToDetach)
  {
    C->sendDetachReason(notification::Detached::DetachMode::Detach);
    Server.clientDetachedCallback(*C, *S);
  }

  sendMessage(Client.getControlSocket(), Resp);
}

HANDLER(signalSession)
{
  (void)Server;
  MSG(request::Signal);
  SessionData* S = Client.getAttachedSession();
  if (!S || !S->hasProcess())
    return;
  S->getProcess().signal(Msg->SigNum);
}

HANDLER(redrawNotified)
{
  (void)Server;
  MSG(notification::Redraw);

  SessionData* S = Client.getAttachedSession();
  if (!S)
    return;
  if (S->hasProcess() && S->getProcess().hasPty())
    S->getProcess().getPty()->setSize(Msg->Rows, Msg->Columns);
}

HANDLER(statisticsRequest)
{
  MSG(request::Statistics);
  sendMessage(Client.getControlSocket(),
              response::Statistics{Server.statistics()});
}

#undef HANDLER

} // namespace monomux::server

#undef LOG
