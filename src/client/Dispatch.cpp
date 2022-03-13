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
#include "Client.hpp"

#include "control/Message.hpp"
#include "system/unreachable.hpp"

namespace monomux
{

void Client::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME)                                                           \
  std::bind(std::mem_fn(&Client::NAME), this, std::placeholders::_1)
#define DISPATCH(K, FUNCTION) Dispatch.try_emplace(KIND(K), MEMBER(FUNCTION));
#include "Dispatch.ipp"
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME) void Client::NAME(std::string_view Message)

HANDLER(responseClientID)
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;

  auto R = response::ClientID::decode(Message);
  if (!R.has_value())
    return;

  ClientID = R->Client.ID;
  Nonce.emplace(R->Client.Nonce);

  std::clog << "DEBUG: Client is " << ClientID << " (with nonce " << *Nonce
            << ')' << std::endl;
}

HANDLER(responseDataSocket)
{
  (void)Message;

  // This handler is a DNI placeholder. The handler for this message is
  // hard-coded into the handshake process. The listener that handles messages
  // should never listen on the data socket, and should not receive a message
  // like this again, after a successful handshake!
  unreachable("DataSocketResponse handler should not fire automatically!");
}

#undef HANDLER

} // namespace monomux
