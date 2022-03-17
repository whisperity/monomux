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
#include "Environment.hpp"
#include "CheckedPOSIX.hpp"
#include "POD.hpp"

#include <cstdlib>

#include <sys/stat.h>

namespace monomux
{

std::string getEnv(const std::string& Key)
{
  const char* const Value = std::getenv(Key.c_str());
  if (!Value)
    return {};
  return {Value};
}

std::string defaultShell()
{
  std::string EnvVar = getEnv("SHELL");
  if (!EnvVar.empty())
    return EnvVar;

  // Try to see if /bin/bash is available and default to that.
  auto Bash = CheckedPOSIX(
    [] {
      POD<struct ::stat> Stat;
      return ::stat("/bin/bash", &Stat);
    },
    -1);
  if (Bash)
    return "/bin/bash";

  // Try to see if /bin/sh is available and default to that.
  auto Sh = CheckedPOSIX(
    [] {
      POD<struct ::stat> Stat;
      return ::stat("/bin/sh", &Stat);
    },
    -1);
  if (Sh)
    return "/bin/sh";

  // Did not succeed.
  return {};
}

} // namespace monomux
