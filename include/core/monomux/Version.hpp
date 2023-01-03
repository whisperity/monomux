/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>
#include <string>

namespace monomux
{

struct Version
{
  std::size_t Major, Minor, Patch, Build;
  std::size_t Offset;
  std::string Commit;
  bool IsDirty;
};

/// \returns the full version information produced by the build system.
[[nodiscard]] Version getVersion();

/// \returns a short version string, e.g. \p 1.0.0
[[nodiscard]] std::string getShortVersion();

/// \returns a full version string, including additional bits, if any.
[[nodiscard]] std::string getFullVersion();

} // namespace monomux
