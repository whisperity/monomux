/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include "monomux/Config.h"

/// A generic macro that prints to some \p ostream the prefix for a "platform
/// not supported" message.
#define MONOMUX_FEED_PLATFORM_NOT_SUPPORTED_MESSAGE                            \
  "ERROR: The current platform " << '(' << MONOMUX_PLATFORM << ')'             \
                                 << " does not support "

namespace monomux::system
{

enum class PlatformTag
{
  Unknown = MONOMUX_PLATFORM_ID_Unsupported,

  /// Standard UNIX and POSIX systems, most importantly Linux.
  Unix = MONOMUX_PLATFORM_ID_Unix
};

/// Dummy class that is implemented by platform-specific details to provide
/// business logic to \p Handle and keep it as a value-semantics-capable class.
template <PlatformTag> struct HandleTraits
{};

/// Dummy class that is implemented by platform-specific details to provide
/// business logic to \p Process.
template <PlatformTag> struct ProcessTraits
{};

/// Dummy class that is implemented by platform-specific details to provide
/// business logic to \p SignalHandling and keep it as a value-semantics-capable
/// class.
template <PlatformTag> struct SignalTraits
{};

/// Base class for querying and building platform-specific bits of information.
class Platform
{
public:
  /// \returns the default shell (command interpreter) for the current user.
  [[nodiscard]] static std::string defaultShell();

  struct SocketPath
  {
    /// \returns the default directory where a server socket should be placed
    /// for the current user.
    [[nodiscard]] static SocketPath defaultSocketPath();

    /// Transforms the specified \p Path into a split \p SocketPath object.
    [[nodiscard]] static SocketPath absolutise(const std::string& Path);

    /// \returns the \p Path and \p Filename concatenated appropriately.
    [[nodiscard]] std::string
    to_string() const; // NOLINT(readability-identifier-naming)

    std::string Path;
    std::string Filename;

    /// Whether the \p Path value (without the \p Filename) is likely specific
    /// to the current user.
    bool IsPathLikelyUserSpecific;
  };
};

} // namespace monomux::system
