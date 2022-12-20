/**
 * Copyright (C) 2022 Whisperity
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
  static std::string defaultShell();

  struct SocketPath
  {
    /// \returns the default directory where a server socket should be placed
    /// for the current user.
    static SocketPath defaultSocketPath();

    /// Transforms the specified \p Path into a split \p SocketPath object.
    static SocketPath absolutise(const std::string& Path);

    /// \returns the \p Path and \p Filename concatenated appropriately.
    std::string to_string() const; // NOLINT(readability-identifier-naming)

    std::string Path;
    std::string Filename;

    /// Whether the \p Path value (without the \p Filename) is likely specific
    /// to the current user.
    bool IsPathLikelyUserSpecific;
  };
};

} // namespace monomux::system
