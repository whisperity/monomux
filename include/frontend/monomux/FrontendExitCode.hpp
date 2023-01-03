/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

namespace monomux
{

/// Contains the exit codes MonoMux \p main() functions return with.
enum class FrontendExitCode : int
{
  /// Successful execution (processes exited gracefully).
  Success = 0,

  /// Indicates a fatal error in the communication with the server.
  SystemError = 1,

  /// Values specified on the command-line of MonoMux are erroneous.
  InvocationError = 2,

  /// Nonspecific other failure.
  Failure = 3,
};

} // namespace monomux
