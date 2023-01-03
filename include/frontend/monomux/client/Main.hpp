/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "monomux/client/Client.hpp"
#include "monomux/system/Environment.hpp"
#include "monomux/system/Process.hpp"

namespace monomux::client
{

/// Options interested to invocation of a Monomux Client.
struct Options
{
  /// Format the options back into the CLI invocation they were parsed from.
  [[nodiscard]] std::vector<std::string> toArgv() const;

  // (To initialise the bitfields...)
  Options();

  /// \returns if control-mode flags (transmitted to the server through a
  /// non-terminal client) are enabled.
  [[nodiscard]] bool isControlMode() const noexcept;

  /// Whether the client mode was enabled.
  bool ClientMode : 1;

  /// Whether the user requested only listing the sessions on the server, but
  /// no connection to be made.
  bool OnlyListSessions : 1;

  /// Whether the client should start with showing the session selection menu,
  /// and disregard normal startup decision heuristics.
  bool InteractiveSessionMenu : 1;

  /// Whether it was requested to detach the latest client from a session.
  ///
  /// \note This is a control-mode flag.
  bool DetachRequestLatest : 1;

  /// Whether it was requested to detach all clients from a session.
  ///
  /// \note This is a control-mode flag.
  bool DetachRequestAll : 1;

  /// Whether it was requested to gather statistics from the running server.
  ///
  /// \note This is a control-mode flag.
  bool StatisticsRequest : 1;

  /// The path to the server socket where the client should connect to.
  std::optional<std::string> SocketPath;

  /// The name of the session the client should create if does not exist, or
  /// attach to if exists.
  std::optional<std::string> SessionName;

  /// The options of the programs to start if a new session is created during
  /// the client's connection. (Ignored if the client attaches to an existing
  /// session.)
  std::optional<system::Process::SpawnOptions> Program;

  /// Contains the master connection to the server, if such was established.
  std::optional<Client> Connection;

  /// If the client has been invoked within a session and we were able to deduce
  /// this, store the session data.
  std::optional<system::MonomuxSession> SessionData;
};

/// Returns the socket and session data that should be used based on the
/// provided \p Opts \b and the current environment, e.g., if the process is
/// running within a MonoMux context already.
[[nodiscard]] system::MonomuxSession
getEnvironmentalSession(const Options& Opts);

/// Attempt to establish connection to a Monomux Server specified in \p Opts.
///
/// \param Block Whether to continue retrying the connection and block until
/// success.
/// \param FailureReason If given, after an unsuccessful connection, a
/// human-readable reason for the failure will be written to.
[[nodiscard]] std::optional<Client>
connect(Options& Opts, bool Block, std::string* FailureReason);

/// Attempts to make the \p Client fully featured with a \b Data connection,
/// capable of actually exchanging user-specific information with the server.
///
/// \param FailureReason If given, after an unsuccessful connection, a
/// human-readable reason for the failure will be written to.
[[nodiscard]] bool makeWholeWithData(Client& Client,
                                     std::string* FailureReason);

// FIXME: Some sort of "Either" type here?
/// Executes the "official" Monomux Client frontend logic.
///
/// \returns Either the exit code of the underlying session, or a
/// \p FrontendExitCode.
[[nodiscard]] int main(Options& Opts);

} // namespace monomux::client
