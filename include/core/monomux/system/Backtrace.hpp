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
#include <cstdint>
#include <iosfwd>

namespace monomux::system
{

/// Handler for formatting a raw "Segmentation fault" or "Aborted" crash message
/// into something meaningful that aids with debugging.
class Backtrace
{
public:
  /// The maximum size supported for generating a backtrace. Larger \p Depth
  /// values will be truncated to this value.
  static constexpr std::size_t MaxSize = 512;

protected:
  /// Creates a backtrace from the current function with at most \p Depth
  /// stack \p Frame visited. The first \p Ignored stack frames will be
  /// skipped from the report.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  Backtrace(std::size_t /*Depth*/ = MaxSize, std::size_t Ignore = 0)
    : IgnoredFrameCount(Ignore)
  {}

public:
  virtual ~Backtrace() = default;

  /// Prettify the stack symbol information and fill \p Pretty for each \p Frame
  /// by calling system binaries such as \p addr2line on the collected raw data.
  virtual void prettify() = 0;

  const std::size_t IgnoredFrameCount;
};

/// Prints \p Trace to the output \p OS using the default formatting logic.
void printBacktrace(std::ostream& OS, const Backtrace& Trace);

/// Generate a backtrace right now, and print it to \p OS with the default
/// formatting logic.
void printBacktrace(std::ostream& OS, bool Prettify = true);

} // namespace monomux::system
