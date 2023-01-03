/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "monomux/FrontendExitCode.hpp"

namespace monomux::server
{

/// Options interested to invocation of a Monomux Server.
struct Options
{
  /// Format the options back into the CLI invocation they were parsed from.
  [[nodiscard]] std::vector<std::string> toArgv() const;

  // (To initialise the bitfields...)
  Options();

  /// Whether the server mode was enabled.
  bool ServerMode : 1;

  /// Whether the server should run as a background process.
  bool Background : 1;

  /// Whether the server should automatically quit if the last session running
  /// has terminated.
  bool ExitOnLastSessionTerminate : 1;

  /// The path of the server socket to start listening on.
  std::optional<std::string> SocketPath;
};

/// \p exec() into a server process that is created with the \p Opts options.
[[noreturn]] void exec(const Options& Opts, const char* ArgV0);

/// Executes the "official" Monomux Server frontend logic.
[[nodiscard]] FrontendExitCode main(Options& Opts);

} // namespace monomux::server
