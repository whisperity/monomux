/**
 *Copyright (C) 2022 Whisperity
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
#pragma once
#include <chrono>
#include <memory>
#include <optional>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/message/Message.hpp"
#include "monomux/system/Socket.hpp"

namespace monomux::server
{

class SessionData;

/// Stores information about and associated resources to a connected client.
class ClientData
{
public:
  ClientData(std::unique_ptr<system::Socket> Connection);

  [[nodiscard]] std::size_t id() const noexcept { return ID; }
  /// Returns the most recent random-generated nonce for this client, and
  /// clear it from memory, rendering it useless in later authentications.
  [[nodiscard]] std::size_t consumeNonce() noexcept;
  /// Creates a new random number for the client's authentication, and returns
  /// it. The value is stored for until used.
  [[nodiscard]] std::size_t makeNewNonce() noexcept;

  [[nodiscard]] std::chrono::time_point<std::chrono::system_clock>
  whenCreated() const noexcept
  {
    return Created;
  }
  [[nodiscard]] std::chrono::time_point<std::chrono::system_clock>
  lastActive() const noexcept
  {
    return LastActivity;
  }
  void activity() noexcept { LastActivity = std::chrono::system_clock::now(); }

  [[nodiscard]] system::Socket& getControlSocket() noexcept
  {
    return *ControlConnection;
  }
  [[nodiscard]] system::Socket* getDataSocket() noexcept
  {
    return DataConnection.get();
  }

  /// Releases the control socket of the other client and associates it as the
  /// data connection of the current client.
  void subjugateIntoDataSocket(ClientData& Other) noexcept;

  [[nodiscard]] const SessionData* getAttachedSession() const noexcept
  {
    return AttachedSession;
  }
  MONOMUX_MEMBER_0(SessionData*, getAttachedSession, [[nodiscard]], noexcept);
  void detachSession() noexcept { AttachedSession = nullptr; }
  void attachToSession(SessionData& Session) noexcept
  {
    AttachedSession = &Session;
  }

  /// Sends the specified detachment reason to the client, if it is connected.
  ///
  /// \param EC The exit code of the session that is detaching from. Not always
  /// meaningful.
  /// \param Reason The reason behind the detachment. Not always meaningful.
  void sendDetachReason(monomux::message::notification::Detached::DetachMode R,
                        int EC = 0,
                        std::string Reason = {});

private:
  std::size_t ID;
  std::optional<std::size_t> Nonce;
  /// The timestamp when the client connected.
  std::chrono::time_point<std::chrono::system_clock> Created;
  /// The timestamp when the client was most recently trasmitting \b data.
  std::chrono::time_point<std::chrono::system_clock> LastActivity;

  /// The control connection transcieves control information and commands.
  std::unique_ptr<system::Socket> ControlConnection;

  /// The data connection transcieves the actual program data.
  std::unique_ptr<system::Socket> DataConnection;

  /// \e If the client is attached to a session, points to the data record of
  /// the session.
  SessionData* AttachedSession;
};

} // namespace monomux::server
