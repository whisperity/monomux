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
#include "monomux/Log.hpp"
#include "monomux/client/Client.hpp"
#include "monomux/control/Message.hpp"
#include "monomux/unreachable.hpp"

#define LOG(SEVERITY) monomux::log::SEVERITY("client/Dispatch")

using namespace monomux::message;

namespace monomux::client
{

void Client::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME) &Client::NAME
#define DISPATCH(K, FUNCTION) registerMessageHandler(KIND(K), MEMBER(FUNCTION));
#include "monomux/client/Dispatch.ipp"
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME)                                                          \
  void Client::NAME(Client& Client, std::string_view Message)

#define MSG(TYPE)                                                              \
  std::optional<TYPE> Msg = TYPE::decode(Message);                             \
  if (!Msg)                                                                    \
    return;                                                                    \
  MONOMUX_TRACE_LOG(LOG(trace) << __PRETTY_FUNCTION__);

HANDLER(responseClientID)
{
  MSG(response::ClientID);

  Client.ClientID = Msg->Client.ID;
  Client.Nonce.emplace(Msg->Client.Nonce);

  MONOMUX_TRACE_LOG(LOG(data) << "Client is \"" << Client.ClientID
                              << "\" with nonce: " << *Client.Nonce);
}

HANDLER(receivedDetachNotification)
{
  using namespace monomux::message::notification;
  MSG(notification::Detached);

  switch (Msg->Mode)
  {
    case Detached::Detach:
      Client.exit(Detached, 0, "");
      break;
    case Detached::Exit:
      Client.exit(SessionExit, Msg->ExitCode, "");
      break;
    case Detached::ServerShutdown:
      Client.exit(ServerExit, 0, "");
      break;
    case Detached::Kicked:
      Client.exit(ServerKicked, 0, std::move(Msg->Reason));
      break;
  }
}

#undef HANDLER

} // namespace monomux::client

#undef MSG
#undef HANDLER
#undef LOG
