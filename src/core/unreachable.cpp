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
#include <cstdio>
#include <cstdlib>

#include "monomux/unreachable.hpp"

[[noreturn]] void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
unreachable_impl(const char* Msg, const char* File, std::size_t LineNo)
{
  (void)fprintf(stderr, "FATAL! UNREACHABLE executed");
  if (File)
    (void)fprintf(stderr, " at %s:%zu", File, LineNo);

  if (Msg)
    (void)fprintf(stderr, ": %s\n", Msg);
  else
    (void)fputc('\n', stderr);

  // [[noreturn]]
  std::abort();
  std::_Exit(-4);
}
