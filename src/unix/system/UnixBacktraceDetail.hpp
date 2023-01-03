/* SPDX-License-Identifier: LGPL-3.0-only */
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
