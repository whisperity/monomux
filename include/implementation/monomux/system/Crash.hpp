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
#include <string>
#include <string_view>
#include <vector>

namespace monomux
{

/// Handler for formatting a raw "Segmentation fault" or "Aborted" crash message
/// into something meaningful that aids in debugging.
class Backtrace
{
public:
  struct Frame
  {
    std::size_t Index;

    /// Each item in the array pointed to by \p buffer is of type \p void*, and
    /// is the return address from the corresponding stack frame.
    const void* Address;

    /// The offset in the raw image (as opposed to its memory-loaded version)
    /// for the symbol's address. This value is used during symbolisation to
    /// find the appropriate symbol data.
    ///
    /// \see prettify()
    const void* ImageOffset;

    /// The symbolic representation of each address consists of the function
    /// name (if this can be determined), a hexademical offset into the
    /// function, and the actual return address (in hexadecimal).
    std::string_view SymbolData;

    /// The hexadecimal address string of the instruction in the loaded binary.
    std::string_view HexAddress;
    /// The name of the binary the symbol is loaded from.
    std::string_view Binary;
    /// The name of the symbol.
    std::string_view Symbol;
    /// The hexadecimal offset from the symbol label (or if there is no symbol,
    /// from the start of the image) for the instruction of the stack frame.
    std::string_view Offset;

    /// The prettified version of the raw \p SymbolData, returned by symbolisers
    /// such as \p addr2line or \p llvm-symbolizer.
    std::string Pretty;
  };

  /// The maximum size supported for generating a backtrace. Larger \p Depth
  /// values will be truncated to this value.
  static constexpr std::size_t MaxSize = 512;

  /// Creates a backtrace from the current function with at most \p Depth
  /// stack \p Frame visited. The first \p Ignored stack frames will be
  /// skipped from the report.
  Backtrace(std::size_t Depth = MaxSize, std::size_t Ignore = 0);

  ~Backtrace();

  const std::vector<Frame>& getFrames() const noexcept { return Frames; }

  /// Prettify the stack symbol information and fill \p Pretty for each \p Frame
  /// by calling system binaries such as \p addr2line on the collected raw data.
  void prettify();

private:
  std::vector<Frame> Frames;

  /// The buffer where the backtrace generator returns the symbol information
  /// in a string format.
  const char* const* SymbolDataBuffer;
};

/// Prints \p Trace to the output \p OS using a default formatting logic.
void printBacktrace(std::ostream& OS, const Backtrace& Trace);

/// Generate a backtrace and print it to \p OS.
void printBacktrace(std::ostream& OS, bool Prettify = true);

} // namespace monomux
