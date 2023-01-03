/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/system/Process.hpp"

namespace monomux::server
{

class ClientData;

/// Encapsulates a running session under the server owning the instance.
class SessionData
{
public:
  SessionData(std::string Name)
    : Name(std::move(Name)), Created(std::chrono::system_clock::now())
  {}

  [[nodiscard]] const std::string& name() const noexcept { return Name; }
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

  [[nodiscard]] bool hasProcess() const noexcept
  {
    return static_cast<bool>(MainProcess);
  }
  void setProcess(std::unique_ptr<system::Process>&& Process) noexcept;
  [[nodiscard]] const system::Process& getProcess() const noexcept
  {
    assert(hasProcess());
    return *MainProcess;
  }
  MONOMUX_MEMBER_0(system::Process&, getProcess, [[nodiscard]], noexcept);

  /// \returns a file descriptor that can be used as a key to identify
  /// the connection towards the session.
  [[nodiscard]] system::Handle::Raw getIdentifyingHandle() const noexcept;

  /// \returns the connection through which data can be read from the session,
  /// if there is any.
  [[nodiscard]] system::Pipe* getReader() noexcept
  {
    if (!hasProcess() || !getProcess().hasPty())
      return nullptr;
    return &getProcess().getPty()->reader();
  }
  /// \returns the connection through which data can be sent to the session,
  /// if there is any.
  [[nodiscard]] system::Pipe* getWriter() noexcept
  {
    if (!hasProcess() || !getProcess().hasPty())
      return nullptr;
    return &getProcess().getPty()->writer();
  }

  [[nodiscard]] const std::vector<ClientData*>&
  getAttachedClients() const noexcept
  {
    return AttachedClients;
  }
  /// \returns the \p ClientData from all attached client which \p activity()
  /// field is the newest (most recently active client).
  [[nodiscard]] ClientData* getLatestClient() const;
  void attachClient(ClientData& Client);
  void removeClient(ClientData& Client) noexcept;

private:
  /// A user-given identifier for the session.
  std::string Name;
  /// The timestamp when the session was spawned.
  std::chrono::time_point<std::chrono::system_clock> Created;
  /// The timestamp when the underlying program was most recently trasmitted
  /// data.
  std::chrono::time_point<std::chrono::system_clock> LastActivity;

  /// The process (if any) executing in the session.
  ///
  /// \note This is only set when the session is spawned with a process, and
  /// stores no information about the underlying process, outside of Monomux's
  /// control, changing its image via an \p exec() call...
  std::unique_ptr<system::Process> MainProcess;

  /// The list of clients currently attached to this session.
  std::vector<ClientData*> AttachedClients;
};

} // namespace monomux::server
