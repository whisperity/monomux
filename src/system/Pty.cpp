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
#include <linux/limits.h>
#include <pty.h>
#include <unistd.h>
#include <utmp.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/Pty.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Pty")

namespace monomux
{

Pty::Pty()
{
  raw_fd MasterFD;
  raw_fd SlaveFD;
  POD<char[PATH_MAX]> DeviceName;

  CheckedPOSIXThrow(
    [&MasterFD, &SlaveFD, &DeviceName] {
      return ::openpty(&MasterFD, &SlaveFD, DeviceName, nullptr, nullptr);
    },
    "Failed to openpty()",
    -1);

  LOG(trace) << "Opened " << DeviceName << " (master: " << MasterFD
             << ", slave: " << SlaveFD << ')';

  Master = MasterFD;
  Slave = SlaveFD;
  Name = DeviceName;
}

void Pty::setupParentSide()
{
  MONOMUX_TRACE_LOG(LOG(trace) << Master << ": Set up as parent...");

  // Close PTS, the slave PTY.
  raw_fd PTS = Slave.release();
  fd::close(PTS);

  IsMaster = true;
  fd::setNonBlockingCloseOnExec(Master);
}

void Pty::setupChildrenSide()
{
  MONOMUX_TRACE_LOG(LOG(trace) << Slave << ": Set up as child...");

  // Closes PTM, the pseudoterminal multiplexer master (PTMX).
  raw_fd PTM = Master.release();
  fd::close(PTM);

  CheckedPOSIXThrow(
    [this] { return ::login_tty(Slave); }, "login_tty in child", -1);
}

void Pty::setSize(unsigned short Rows, unsigned short Columns)
{
  if (!isMaster())
    throw std::invalid_argument{"setSize() not allowed on slave device."};

  MONOMUX_TRACE_LOG(LOG(data) << Master << ": setSize(Rows=" << Rows
                              << ", Columns=" << Columns << ')');

  POD<struct ::winsize> Size;
  Size->ws_row = Rows;
  Size->ws_col = Columns;
  CheckedPOSIXThrow(
    [RawFD = Master.get(), &Size] { return ::ioctl(RawFD, TIOCSWINSZ, &Size); },
    "ioctl(PTMX, TIOCSWINSZ /* set window size*/);",
    -1);
}

} // namespace monomux
