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
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "monomux/system/Backtrace.hpp"

namespace monomux::system::unix
{

class Backtrace : public system::Backtrace
{
public:
  /// Contains the results of the \p backtrace() operation as raw sub-strings
  /// into the runtime's output.
  ///
  /// \note These views are valid only if the \p SymbolDataBuffer is valid.
  struct RawData
  {
    /// The symbolic representation of each address consists of the function
    /// name (if this can be determined), a hexademical offset into the
    /// function, and the actual return address (in hexadecimal).
    std::string_view Full;

    /// The hexadecimal address string of the instruction in the loaded binary.
    std::string_view HexAddress;
    /// The name of the binary the symbol is loaded from.
    std::string_view Binary;
    /// The name of the symbol.
    std::string_view Symbol;
    /// The hexadecimal offset from the symbol label (or if there is no symbol,
    /// from the start of the image) for the instruction of the stack frame.
    std::string_view Offset;
  };

  struct Symbol
  {
    /// The symbol name associated with the frame, as returned by a symboliser.
    std::string Name;
    /// The location extracted from the executing image where the frame's
    /// executed instruction is compiled from. Requires the existence of debug
    /// information.
    std::string Filename;
    std::size_t Line, Column;

    /// Symbols might be inlined into each other if an optimised build is
    /// set up. If such is true for \p this then this field will contain the
    /// data for the inlining, potentially recursively.
    std::unique_ptr<Symbol> InlinedBy;

    /// Merges symbol information in a sensible fashion from another symboliser
    /// result.
    void mergeFrom(Symbol&& RHS);

    /// Creates the inlining symbol's instance and returns it for data filling.
    [[nodiscard]] Symbol& startInlineInfo();

    /// \returns whether the Symbol instance, or anything in the \p InlinedBy
    /// chain conveys meaningful information.
    [[nodiscard]] bool hasMeaningfulInformation() const;
  };

  struct Frame
  {
    /// The index of the stack frame. This is decremented, so the latest stack
    /// frame has the highest index.
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

    /// Contains the raw data returned by the runtime about the machine-level
    /// symbol information.
    RawData Data;

    /// The location of the executed instruction, in the source code, if such
    /// information was available. This is conditional on having a debug-like
    /// build, access to the source files in the debug information, and a
    /// symboliser (e.g., \p addr2line) capable of retrieving these details.
    std::optional<Symbol> Info;
  };

  /// Creates a backtrace from the current function with at most \p Depth
  /// stack \p Frame visited. The first \p Ignored stack frames will be
  /// skipped from the report.
  Backtrace(std::size_t Depth = MaxSize, std::size_t Ignore = 0);

  ~Backtrace() override;

  void prettify() override;

  /// \returns the stack frames created and stored when the \p Backtrace
  /// instance was constructed. These frames are allocated in the usual order,
  /// with the most recent stack frame being the first (index 0) in the vector.
  [[nodiscard]] const std::vector<Frame>& getFrames() const noexcept
  {
    return Frames;
  }

private:
  std::vector<Frame> Frames;

  /// The buffer where the backtrace generator returns the symbol information
  /// in a string format.
  const char* const* SymbolDataBuffer;
};

} // namespace monomux::system::unix
