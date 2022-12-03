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
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "monomux/system/UnixBacktrace.hpp"

namespace monomux::system::unix
{

/// Abstract interface for tools that are capable of giving symbol info about a
/// process at run-time.
struct Symboliser
{
  const std::string& name() const noexcept { return Binary; }

  bool check() const;

  virtual std::vector<std::optional<Backtrace::Symbol>>
  symbolise(const std::string& Object,
            const std::vector<Backtrace::Frame*>& Frames) const = 0;

  virtual ~Symboliser() = default;

  static void noSymbolisersMessage();

protected:
  std::string Binary;
  Symboliser(std::string Binary) : Binary(std::move(Binary)) {}
};

std::vector<std::unique_ptr<Symboliser>> makeSymbolisers();

/// Implement a default backtrace formatting logic that pretty-prints the
/// frames of a \p Backtrace in an orderly, numbered fashion.
class DefaultBacktraceFormatter
{
public:
  DefaultBacktraceFormatter(std::ostream& OS, const Backtrace& Trace);

  void print();

private:
  std::ostream& OS;
  const Backtrace& Trace;

  const std::size_t FrameDigitsBase10;

  /// The size of the prefix printed before each frame.
  std::size_t PrefixLen;

  bool IsPrintingInlineInfo = false;

  void prefix();

  void formatRawSymbolData(const Backtrace::RawData& D, bool PrintName);

  void formatPrettySymbolData(const Backtrace::Frame& F);

  void formatPrettySymbolInliningData(const Backtrace::Symbol& S);

  void printSourceCode(const Backtrace::Symbol& S);
};

} // namespace monomux::system::unix
