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
#include <unistd.h>
#include <utmp.h>

namespace monomux
{

Pty::Pty()
{
  raw_fd MasterFD, SlaveFD;
  char DEVICE_NAME[1024];

  CheckedPOSIXThrow(
    [&MasterFD, &SlaveFD, &DEVICE_NAME]() {
      return ::openpty(&MasterFD, &SlaveFD, DEVICE_NAME, nullptr, nullptr);
    },
    "Failed to openpty()",
    -1);

  std::clog << "FDs: " << MasterFD << ' ' << SlaveFD << std::endl;
  std::clog << "Device: " << DEVICE_NAME << std::endl;

  Master.emplace(Socket::wrap(MasterFD));
  Slave.emplace(Socket::wrap(SlaveFD));
}

void Pty::setupParentSide()
{
  std::clog << "In parent: " << Master->raw() << ' ' << Slave->raw()
            << std::endl;
  Slave.reset(); // Close slave PTY.
}

void Pty::setupChildrenSide()
{
  std::clog << "In child: " << Master->raw() << ' ' << Slave->raw()
            << std::endl;
  Master.reset(); // Close master PTY.
  CheckedPOSIXThrow(
    [this] { return ::login_tty(Slave->raw()); }, "login_tty in child", -1);
}


} // namespace monomux
