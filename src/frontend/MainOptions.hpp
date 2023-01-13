/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <cstdint>

#include "monomux/Log.hpp"

namespace monomux
{

struct MainOptions
{
  /// \p -h
  bool ShowHelp : 1;

  /// \p -V
  bool ShowVersion : 1;

  /// \p -V a second time
  bool ShowElaborateBuildInformation : 1;

  /// \p -v
  bool AnyVerboseFlag : 1;
  /// \p -q
  bool AnyQuietFlag : 1;

  /// \p -v or \p -q sequences
  std::int8_t VerbosityQuietnessDifferential = 0;

  /// \p -v and \p -q translated to \p Severity choice.
  monomux::log::Severity Severity;
};

} // namespace monomux
