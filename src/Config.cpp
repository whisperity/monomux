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
#include <sstream>
#include <string_view>

#include "Config.hpp"

namespace monomux
{

std::string getHumanReadableConfiguration()
{
  std::ostringstream Buf;

  Buf << " * " << MONOMUX_BUILD_TYPE << " build\n";

#ifdef MONOMUX_BUILD_SHARED_LIBS
  Buf << " * SHARED (dynamic) library\n";
#else /* !MONOMUX_BUILD_SHARED_LIBS */
#ifdef MONOMUX_BUILD_UNITY
  Buf << " * UNITY library\n";
#else  /* !MONOMUX_BUILD_UNITY */
  Buf << " * STATIC library\n";
#endif /* MONOMUX_BUILD_UNITY */
#endif /* MONOMUX_BUILD_SHARED_LIBS */

#ifndef MONOMUX_NON_ESSENTIAL_LOGS
  Buf << " - Non-essential trace logs\n";
#else  /* !MONOMUX_NON_ESSENTIAL_LOGS */
  Buf << " + Non-essential trace logs\n";
#endif /* MONOMUX_NON_ESSENTIAL_LOGS */

  std::string S = Buf.str();

  {
    // Clean up multiple subsequent newlines from the output.
    static constexpr std::string_view DoubleNewline = "\n\n";
    static constexpr std::string_view SingleNewline = "\n";
    std::string::size_type P = 0;
    while ((P = S.find(DoubleNewline.data())) != std::string::npos)
      S.replace(
        P, DoubleNewline.size(), SingleNewline.data(), SingleNewline.size());
  }

  return S;
}

} // namespace monomux
