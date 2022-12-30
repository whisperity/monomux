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
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace monomux
{

/// Formats the given \p Chrono \p Time object to an internationally viable
/// representation.
template <typename T>[[nodiscard]] std::string formatTime(const T& Time)
{
  std::time_t RawTime = T::clock::to_time_t(Time);
  std::tm SplitTime = *std::localtime(&RawTime);

  std::ostringstream Buf;
  // Mirror the behaviour of tmux/byobu menu.
  Buf << std::put_time(&SplitTime, "%a %b %e %H:%M:%S %Y");
  return Buf.str();
}

} // namespace monomux
