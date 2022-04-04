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
Version getVersion();

/// \returns a short version string, e.g. \p 1.0.0
std::string getShortVersion();

/// \returns a full version string, including additional bits, if any.
std::string getFullVersion();

} // namespace monomux
