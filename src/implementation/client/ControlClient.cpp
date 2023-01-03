/* SPDX-License-Identifier: LGPL-3.0-only */
#include "monomux/message/Message.hpp"
#include "monomux/message/PascalString.hpp"

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
