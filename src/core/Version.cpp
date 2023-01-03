/* SPDX-License-Identifier: LGPL-3.0-only */
#include <cstring>
#include <sstream>

#include "monomux/Version.h"

#include "monomux/Version.hpp"

namespace monomux
{

Version getVersion()
{
  Version V{};
  V.Major = std::stoull(MONOMUX_VERSION_MAJOR);
  V.Minor = std::stoull(MONOMUX_VERSION_MINOR);
  V.Patch = std::stoull(MONOMUX_VERSION_PATCH);
  V.Build = std::stoull(MONOMUX_VERSION_TWEAK);
  V.Offset = 0;
  V.IsDirty = false;

#ifdef MONOMUX_VERSION_HAS_EXTRAS
  V.Offset = std::stoull(MONOMUX_VERSION_OFFSET);
  V.Commit = MONOMUX_VERSION_COMMIT;
  V.IsDirty = std::strlen(MONOMUX_VERSION_DIRTY) > 0;
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
