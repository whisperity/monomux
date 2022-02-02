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
#include "Pty.hpp"
#include "CheckedPOSIX.hpp"

#include <iostream>

#include <pty.h>

namespace monomux
{

Pty::Pty()
{
  char DEVICE_NAME[1024];

  CheckedPOSIXThrow(
    [this, &DEVICE_NAME]() {
      return ::openpty(&Master, &Slave, DEVICE_NAME, nullptr, nullptr);
    },
    "Failed to openpty()",
    -1);

  std::clog << Master << ' ' << Slave << std::endl;
  std::clog << DEVICE_NAME << std::endl;
}


} // namespace monomux
