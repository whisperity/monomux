/* SPDX-License-Identifier: LGPL-3.0-only */
#include "monomux/client/Client.hpp"
#include "monomux/message/Message.hpp"
#include "monomux/unreachable.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("client/Dispatch")

namespace monomux::client
{

void Client::setUpDispatch()
{
  using namespace monomux::message;

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
  using namespace monomux::message;                                            \
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
      Client.exit(Exit::Detached, 0, "");
      break;
    case Detached::Exit:
      Client.exit(Exit::SessionExit, Msg->ExitCode, "");
      break;
    case Detached::ServerShutdown:
      Client.exit(Exit::ServerExit, 0, "");
      break;
    case Detached::Kicked:
      Client.exit(Exit::ServerKicked, 0, std::move(Msg->Reason));
      break;
  }
}

#undef HANDLER
#undef MSG

} // namespace monomux::client

#undef LOG
