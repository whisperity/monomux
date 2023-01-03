/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace monomux::message
{

/// A global enumeration table of messages that are supported by the protocol.
/// For each entry, an appropriate struct in namespace \p monomux::request or
/// \p monomux::response defines the data members of the message.
enum class MessageKind : std::uint16_t
{
  /// Indicates a broken message that failed to read as a proper entity.
  Invalid = 0,

  /// Indicates a subobject of a \p Message that cannot be understood
  /// individually.
  Base,

  /// A status message from the server to the client about the details of the
  /// established connection.
  ConnectionNotification,

  /// A request to the server to reply the client's ID to the client.
  ClientIDRequest,
  /// A response for the \p ClientIDRequest, containing the client's ID.
  ClientIDResponse,

  /// A request to the server to associate the connection with another client,
  /// marking it as the data connection/socket.
  DataSocketRequest,
  /// A response for the \p DataSocketRequest, indicating whether the
  /// request was accepted.
  DataSocketResponse,

  /// A request to the server to reply with data about the running sessions on
  /// the server.
  SessionListRequest,
  /// A response to the \p SessionListRequest, containing \p Session data.
  SessionListResponse,

  /// A request to the server to create a brand new new session.
  MakeSessionRequest,
  /// A reponse to the \p MakeSessionRequest containing the results of the new
  /// session.
  MakeSessionResponse,

  /// A request to the server to have the sending client attached to a session.
  AttachRequest,
  /// A response to the \p AttachRequest containing whether the attaching
  /// succeeded.
  AttachResponse,

  /// A request to the server to detach one or more clients from a session.
  DetachRequest,
  /// A response to the \p DetachRequest containing the result of the operation.
  DetachResponse,

  /// A notification sent by the server to a client indicating that the client
  /// had been detached.
  DetachedNotification,

  /// A request to the server to send a \p signal to the attached session.
  SignalRequest,

  /// A notification to the server to apply window resize/redraw to an attached
  /// session.
  RedrawNotification,

  /// A request to the server to respond with statistical information about the
  /// execution.
  StatisticsRequest,
  /// A response to the \p StatisticsRequest.
  StatisticsResponse,
};

/// Helper class that contains the parsed \p MessageKind of a \p Message, and
/// the remaining, not yet parsed \p Buffer.
struct Message
{
  MessageKind Kind;
  std::string_view RawData;

  /// Encodes the given number as a platform-specific binary-string.
  [[nodiscard]] static std::string sizeToBinaryString(std::size_t N);

  /// Decodes a \p std::size_t from the given \p Str.
  [[nodiscard]] static std::size_t
  binaryStringToSize(std::string_view Str) noexcept;

  /// Encodes the \p Kind variable as a binary string for appropriately
  /// prefixing a transmissible buffer.
  [[nodiscard]] std::string encodeKind() const;

  /// Decodes the binary prefix of a message as a \p Kind.
  [[nodiscard]] static MessageKind decodeKind(std::string_view Str) noexcept;

  /// Pack a raw and encoded message into a full transmissible payload.
  [[nodiscard]] std::string pack() const;

  /// Unpack an encoded and fully read payload into its base constitutents.
  [[nodiscard]] static Message unpack(std::string_view Str) noexcept;
};

/// Encodes a message object into its raw data form.
template <typename T>[[nodiscard]] std::string encode(const T& Msg)
{
  std::string RawForm = T::encode(Msg);

  Message MB;
  MB.Kind = Msg.Kind;
  MB.RawData = RawForm;

  return MB.pack();
}

/// Encodes a message object into its raw data form, prefixed with a payload
/// size.
template <typename T>[[nodiscard]] std::string encodeWithSize(const T& Msg)
{
  std::string Payload = encode(Msg);
  return Message::sizeToBinaryString(Payload.size()) + std::move(Payload);
}

/// Decodes the given received buffer as a specific message object, and returns
/// it if successful.
template <typename T>
[[nodiscard]] std::optional<T> decode(std::string_view Str) noexcept
{
  Message MB = Message::unpack(Str);
  if (MB.Kind == MessageKind::Invalid)
    return std::nullopt;

  std::optional<T> Msg = T::decode(MB.RawData);
  return Msg;
}

} // namespace monomux::message
