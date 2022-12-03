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
#include <cassert>

#include "monomux/message/PascalString.hpp"

#include "monomux/server/ClientData.hpp"

namespace monomux::server
{

ClientData::ClientData(std::unique_ptr<system::Socket> Connection)
  : ID(Connection->raw()), Created(std::chrono::system_clock::now()),
    ControlConnection(std::move(Connection)), AttachedSession(nullptr)
{}

static std::size_t NonceCounter = 0; // FIXME: Remove this.

std::size_t ClientData::consumeNonce() noexcept
{
  std::size_t N = Nonce.value_or(0);
  Nonce.reset();
  return N;
}

std::size_t ClientData::makeNewNonce() noexcept
{
  // FIXME: Better random number generation.
  Nonce.emplace(++NonceCounter);
  return *Nonce;
}

void ClientData::subjugateIntoDataSocket(ClientData& Other) noexcept
{
  assert(!DataConnection && "Current client already has a data connection!");
  assert(!Other.DataConnection &&
         "Other client already has a data connection!");
  DataConnection.swap(Other.ControlConnection);
  assert(!Other.ControlConnection && "Other client stayed alive");
}

void ClientData::sendDetachReason(
  monomux::message::notification::Detached::DetachMode R,
  int EC,
  std::string Reason)
{
  message::sendMessage(
    getControlSocket(),
    monomux::message::notification::Detached{R, EC, std::move(Reason)});
}

} // namespace monomux::server
