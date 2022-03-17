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
#include "system/fd.hpp"

namespace monomux
{
namespace client
{

class Terminal
{
  raw_fd In;
  raw_fd Out;

public:
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  Terminal(raw_fd InputStream, raw_fd OutputStream)
    : In(InputStream), Out(OutputStream)
  {}

  raw_fd input() const noexcept { return In; }
  raw_fd output() const noexcept { return Out; }
};

} // namespace client
} // namespace monomux
