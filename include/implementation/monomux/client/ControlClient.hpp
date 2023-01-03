/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include "monomux/client/Client.hpp"

namespace monomux::client
{

/// A \p ControlClient is a non-user-facing management wrapper over an
/// established \p Client. It is used to implement special client-like
/// operations.
///
/// \note The control client \e wraps a \p Client instance, and as such, will
/// act as a separate client from the ones the user might have attached to the
/// session with.
class ControlClient
{
public:
  /// Allows operation in a \p ControlClient \b without attaching to a session.
  ControlClient(Client& C);
  /// Attaches the \p ControlClient to \p Session to allow session-specific
  /// operations.
  ControlClient(Client& C, std::string Session);

  [[nodiscard]] const std::string& sessionName() const noexcept
  {
    return SessionName;
  }

  /// Send a request to the server to gracefully detach the latest (in terms of
  /// activity) client from the session.
  ///
  /// \see server::ClientData::lastActive()
  void requestDetachLatestClient();

  /// Send a request to the server to gracefully detach every client from the
  /// session.
  void requestDetachAllClients();

  /// Sends a request to the server to gather statistical information and reply
  /// it back to this \p Client.
  ///
  /// \throws std::runtime_error Thrown if communication with the server failed
  /// and it did not produce a response that the client could understand.
  [[nodiscard]] std::string requestStatistics();

private:
  Client& BackingClient;

  /// The name of the session the controlling client will send requests to.
  std::string SessionName;
};

} // namespace monomux::client
