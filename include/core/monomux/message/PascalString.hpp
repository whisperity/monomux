/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "monomux/message/MessageBase.hpp"
#include "monomux/system/BufferedChannel.hpp"
#include "monomux/system/Channel.hpp"

namespace monomux::message
{

/// Sends a specific message, fully encoded for transportation, on the
/// \p Channel.
///
/// \note This operation \b MAY block.
template <typename T>
std::size_t sendMessage(system::Channel& Channel, const T& Msg)
{
  return Channel.write(encodeWithSize(Msg));
}

/// Sends a specific message, fully encoded for transportation, on the
/// \p Channel.
///
/// \note This operation \b MAY block.
template <typename T>
std::size_t sendMessage(system::BufferedChannel& Channel, const T& Msg)
{
  return Channel.write(encodeWithSize(Msg));
}

/// Reads a size-prefixed payload from the \p Channel.
///
/// \note This operation \b MAY block.
[[nodiscard]] std::string readPascalString(system::Channel& Channel);

/// Reads a size-prefixed payload from the \p Channel.
///
/// \note This operation \b MAY block.
[[nodiscard]] std::string readPascalString(system::BufferedChannel& Channel);

namespace detail
{

template <typename T>[[nodiscard]] std::optional<T> unpack(std::string&& Data)
{
  Message MsgBase = Message::unpack(Data);
  if (MsgBase.Kind != T::Kind)
    return std::nullopt;

  std::optional<T> Msg = T::decode(MsgBase.RawData);
  return Msg;
}

} // namespace detail

/// Reads a fully encoded message from the \p Channel and expects it to be of
/// message type \p T. If the message successfully read, returns it.
///
/// \note This operation \b MAY block. If the message fails to read, or the
/// message is not the \e expected type, the message \b MAY be dropped and lost.
template <typename T>
[[nodiscard]] std::optional<T> receiveMessage(system::Channel& Channel)
{
  std::string Data = readPascalString(Channel);
  return detail::unpack<T>(std::move(Data));
}

/// Reads a fully encoded message from the \p Channel and expects it to be of
/// message type \p T. If the message successfully read, returns it.
///
/// \note This operation \b MAY block. If the message fails to read, or the
/// message is not the \e expected type, the message \b MAY be dropped and lost.
template <typename T>
[[nodiscard]] std::optional<T> receiveMessage(system::BufferedChannel& Channel)
{
  std::string Data = readPascalString(Channel);
  return detail::unpack<T>(std::move(Data));
}

} // namespace monomux::message
