/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "monomux/message/MessageBase.hpp"

#define MONOMUX_MESSAGE(KIND, NAME)                                            \
  static constexpr MessageKind Kind = MessageKind::KIND;                       \
  [[nodiscard]] static std::optional<NAME> decode(std::string_view Buffer);    \
  [[nodiscard]] static std::string encode(const NAME& Object);

#define MONOMUX_MESSAGE_BASE(NAME)                                             \
  static constexpr MessageKind Kind = MessageKind::Base;                       \
  [[nodiscard]] static std::optional<NAME> decode(std::string_view& Buffer);   \
  [[nodiscard]] static std::string encode(const NAME& Object);

namespace monomux::message
{

/// Contains the data members required to identify a connected Client.
struct ClientID
{
  MONOMUX_MESSAGE_BASE(ClientID);

  /// The identity number of the client on the server it has connected to.
  std::size_t ID{};
  /// A single-use number the client can use in other unassociated requests
  /// to prove its identity.
  std::size_t Nonce{};
};

/// A view of the \p Process::SpawnOptions data structure that is sufficient for
/// network transmission.
struct ProcessSpawnOptions
{
  MONOMUX_MESSAGE_BASE(ProcessSpawnOptions);

  /// \see Process::SpawnOptions::Program
  std::string Program;
  /// \see Process::SpawnOptions::Arguments
  std::vector<std::string> Arguments;

  // (We do not wish to deal with std::nullopt stuff...)

  /// The list of environment variables to be set for the spawned process.
  ///
  /// \see Process::SpawnOptions::Environment
  std::vector<std::pair<std::string, std::string>> SetEnvironment;

  /// The list of environment variables to be ignored and unset in the spawned
  /// process, no matter what the server has them set to.
  ///
  /// \see Process::SpawnOptions::Environment
  std::vector<std::string> UnsetEnvironment;
};

/// A view of the \p Server::SessionData data structure that is sufficient for
/// network transmission.
struct SessionData
{
  MONOMUX_MESSAGE_BASE(SessionData);

  /// \see server::SessionData::Name.
  std::string Name;

  /// \see server::SessionData::Created.
  std::time_t Created{};
};

/// A base class for responding boolean values consistently.
struct Boolean
{
  MONOMUX_MESSAGE_BASE(Boolean);

  operator bool() const { return Value; }
  Boolean& operator=(bool V)
  {
    Value = V;
    return *this;
  }

  bool Value{};
};

namespace request
{

/// A request from the client to the server to deliver the identity information
/// to the client.
///
/// This message is sent as the initial handshake after a connection is
/// established.
struct ClientID
{
  MONOMUX_MESSAGE(ClientIDRequest, ClientID);
};

/// A request from the client to the server sent over the data connection to
/// tell the server to register the connection this request is receieved on
/// to be the data connection of a connected \p Client.
///
/// This message is sent as the initial handshake after a connection is
/// established.
struct DataSocket
{
  MONOMUX_MESSAGE(DataSocketRequest, DataSocket);
  monomux::message::ClientID Client;
};

/// A request from the client to the server to advise the client about the
/// sessions available on the server for attachment.
struct SessionList
{
  MONOMUX_MESSAGE(SessionListRequest, SessionList);
};

/// A request from the client to the server to initialise a new session with
/// the specified parameters.
struct MakeSession
{
  MONOMUX_MESSAGE(MakeSessionRequest, MakeSession);
  /// The name to associate with the created session.
  ///
  /// \note This is non-normative, and may be rejected by the server.
  std::string Name;

  /// The options for the program to create in the session.
  ProcessSpawnOptions SpawnOpts;
};

/// A request from the client to the server to attach the client to the
/// specified session.
struct Attach
{
  MONOMUX_MESSAGE(AttachRequest, Attach);
  /// The name of the session to attach to.
  std::string Name;
};

/// A request from a client to the server to detach some clients from an ongoing
/// session.
struct Detach
{
  MONOMUX_MESSAGE(DetachRequest, Detach);
  enum DetachMode
  {
    /// Detach the latest client active in the session.
    Latest,
    /// Detach every client from the session.
    All
  };
  DetachMode Mode = Latest;
};

/// A request from the client to the server to deliver a process signal to the
/// attached session.
struct Signal
{
  MONOMUX_MESSAGE(SignalRequest, Signal);
  /// \see signal(7)
  int SigNum{};
};

/// A request from a client to the server to respond with statistical
/// information.
struct Statistics
{
  MONOMUX_MESSAGE(StatisticsRequest, Statistics);
};

} // namespace request

namespace response
{

/// The response to the \p request::ClientID, sent by the server.
struct ClientID
{
  MONOMUX_MESSAGE(ClientIDResponse, ClientID);
  monomux::message::ClientID Client;
};

/// The response to the \p request::DataSocket, sent by the server.
///
/// This message is sent back through the connection the request arrived.
/// In case of \p Success, this is the last (and only) control message that is
/// passed through what transmogrified into a \e Data connection.
struct DataSocket
{
  MONOMUX_MESSAGE(DataSocketResponse, DataSocket);
  monomux::message::Boolean Success;
};

/// The response to the \p request::SessionList, sent by the server.
struct SessionList
{
  MONOMUX_MESSAGE(SessionListResponse, SessionList);
  std::vector<monomux::message::SessionData> Sessions;
};

/// The response to the \p request::MakeSession,sent by the server.
struct MakeSession
{
  MONOMUX_MESSAGE(MakeSessionResponse, MakeSession);
  monomux::message::Boolean Success;

  /// The name of the created session. This \b MAY \b NOT be the same as the
  /// \e requested \p Name.
  std::string Name;
};

/// The response to the \p request::Attach specifying whether the server
/// accepted the request.
struct Attach
{
  MONOMUX_MESSAGE(AttachResponse, Attach);
  monomux::message::Boolean Success;
  /// Information about the session the client attached to. Only meaningful if
  /// \p Success is \p true.
  SessionData Session;
};

/// The response to the \p request::Detach indicating receipt.
struct Detach
{
  MONOMUX_MESSAGE(DetachResponse, Detach);
};

/// The response ot the \p request::Statistics containing the response data.
struct Statistics
{
  MONOMUX_MESSAGE(StatisticsResponse, Statistics);
  /// The verbatim reply from the server.
  ///
  /// \warning This text is \b NOT meant to be machine-readable, and only useful
  /// for development and debugging by a human!
  std::string Contents;
};

} // namespace response

namespace notification
{

/// A status message sent by the server to the client during connection
/// establishment.
struct Connection
{
  MONOMUX_MESSAGE(ConnectionNotification, Connection);
  monomux::message::Boolean Accepted;
  /// The reason why the connection cannot be established, if \p Accepted is
  /// \p false.
  std::string Reason;
};

/// A notification sent by the server to the client(s) indicating that the
/// client(s) were detached from a session.
struct Detached
{
  MONOMUX_MESSAGE(DetachedNotification, Detached);
  enum DetachMode
  {
    /// The client was gracefully detached upon a request.
    Detach,
    /// The session the client was attached do exited.
    ///
    /// \see ExitCode
    Exit,
    /// The server shut down.
    ServerShutdown,
    /// The server kicked the client because the client or its connection
    /// misbehaved.
    ///
    /// \see Reason
    Kicked,
  };
  DetachMode Mode = Detach;

  /// The exit code of the process running in the session that exited.
  /// Only meaningful if \p Mode is \p Exit.
  int ExitCode{};

  /// The reason behind the server kicking the client ungracefully.
  /// Only meaingful if \p Mode is \p Kicked.
  std::string Reason;
};

/// A notification send by the client to the server indicating that its terminal
/// buffer ("window size") has changed, prompting the server to relay this
/// information into the attached session.
///
/// \see ioctl_tty(4)
struct Redraw
{
  MONOMUX_MESSAGE(RedrawNotification, Redraw);
  unsigned short Rows{};
  unsigned short Columns{};
};

} // namespace notification

} // namespace monomux::message

#undef MONOMUX_MESSAGE
#undef MONOMUX_MESSAGE_BASE
