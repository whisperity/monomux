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
#include "Message.hpp"

#include <cstring>

#define DECODE(NAME) std::optional<NAME> NAME::decode(const std::string& Buffer)
#define ENCODE(NAME) std::string NAME::encode(const NAME& Object)

namespace monomux
{
namespace request
{

ENCODE(SpawnProcess)
{
  std::string Ret = "<SPAWN>";
  Ret.append(Object.ProcessName);
  Ret.push_back('\0');
  Ret.append("</SPAWN>");
  return Ret;
}

DECODE(SpawnProcess)
{
  SpawnProcess Ret;

  auto P = Buffer.find("<SPAWN>");
  if (P == std::string::npos)
    return std::nullopt;
  P += std::strlen("<SPAWN>");

  auto SV = std::string_view{Buffer.data() + P, Buffer.size() - P};
  P = SV.find('\0');
  Ret.ProcessName = std::string{SV.substr(0, P)};
  SV.remove_prefix(P + 1);

  if (SV != "</SPAWN>")
    return std::nullopt;

  return Ret;
}

} // namespace request
} // namespace monomux

#undef ENCODE
#undef DECODE
