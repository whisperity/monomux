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

using namespace monomux::message;

namespace monomux
{
namespace client
{

void Client::setUpDispatch()
{
#define KIND(E) static_cast<std::uint16_t>(MessageKind::E)
#define MEMBER(NAME)                                                           \
  std::bind(std::mem_fn(&Client::NAME), this, std::placeholders::_1)
#define DISPATCH(K, FUNCTION) registerMessageHandler(KIND(K), MEMBER(FUNCTION));
#include "Dispatch.ipp"
#undef MEMBER
#undef KIND
}

#define HANDLER(NAME) void Client::NAME(std::string_view Message)

#define MSG(TYPE)                                                              \
  std::optional<TYPE> Msg = TYPE::decode(Message);                             \
  if (!Msg)                                                                    \
    return;

HANDLER(responseClientID)
{
  std::clog << __PRETTY_FUNCTION__ << std::endl;

  MSG(response::ClientID);

  ClientID = Msg->Client.ID;
  Nonce.emplace(Msg->Client.Nonce);

  std::clog << "DEBUG: Client is " << ClientID << " (with nonce " << *Nonce
            << ')' << std::endl;
}

#undef HANDLER

} // namespace client
} // namespace monomux
