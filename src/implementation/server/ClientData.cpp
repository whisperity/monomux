/* SPDX-License-Identifier: LGPL-3.0-only */
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
