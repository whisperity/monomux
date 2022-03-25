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
#include "Version.hpp"

#include "monomux/Version.h"

#include <sstream>

namespace monomux
{

Version getVersion()
{
  Version V{};
  V.Major = std::stoull(VERSION_MAJOR);
  V.Minor = std::stoull(VERSION_MINOR);
  V.Patch = std::stoull(VERSION_PATCH);
  V.Build = std::stoull(VERSION_TWEAK);
  V.Offset = 0;
  V.IsDirty = false;

#ifdef VERSION_HAS_EXTRAS
  V.Offset = std::stoull(VERSION_OFFSET);
  V.Commit = VERSION_COMMIT;
  V.IsDirty = VERSION_DIRTY;
#endif

  return V;
}

std::string getShortVersion()
{
  std::ostringstream Buf;
  Version V = getVersion();
  Buf << V.Major << '.' << V.Minor;
  if (V.Patch || V.Build)
    Buf << '.' << V.Patch;
  if (V.Build)
    Buf << '.' << V.Build;
  return Buf.str();
}

std::string getFullVersion()
{
  std::ostringstream Buf;
  Version V = getVersion();
  Buf << getShortVersion();
  if (V.Offset || !V.Commit.empty())
    Buf << '+' << V.Offset << '(' << V.Commit << ')';
  if (V.IsDirty)
    Buf << "-dirty!";
  return Buf.str();
}

} // namespace monomux
