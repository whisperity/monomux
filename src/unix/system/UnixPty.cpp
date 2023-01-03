/* SPDX-License-Identifier: LGPL-3.0-only */
#include <sstream>

#include <linux/limits.h>
#include <pty.h>
#include <unistd.h>
#include <utmp.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/system/UnixPipe.hpp"
#include "monomux/system/fd.hpp"

#include "monomux/system/UnixPty.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Pty")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << name() << ": "

namespace monomux::system::unix
{

Pty::Pty()
{
  fd::raw_fd MasterFD;
  fd::raw_fd SlaveFD;
  POD<char[PATH_MAX]> DeviceName;

  CheckedErrnoThrow(
    [&MasterFD, &SlaveFD, &DeviceName] {
      return ::openpty(&MasterFD, &SlaveFD, DeviceName, nullptr, nullptr);
    },
    "Failed to openpty()",
    -1);

  LOG(debug) << "Opened " << DeviceName << " (master: " << MasterFD
             << ", slave: " << SlaveFD << ')';

  Master = Handle::wrap(MasterFD);
  Slave = Handle::wrap(SlaveFD);
  Name = DeviceName;
}

void Pty::setupParentSide()
{
  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << Master << " - set up as parent...");

  // Close PTS, the slave PTY.
  (void)Slave.release();

  IsMaster = true;
  fd::setNonBlockingCloseOnExec(Master);

  std::ostringstream InName;
  std::ostringstream OutName;
  InName << "<r:pty:" << name() << '>';
  OutName << "<w:pty:" << name() << '>';

  Read = std::make_unique<Pipe>(
    Pipe::weakWrap(Master.get(), Pipe::Read, InName.str()));
  Write = std::make_unique<Pipe>(
    Pipe::weakWrap(Master.get(), Pipe::Write, OutName.str()));
}

void Pty::setupChildrenSide()
{
  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << Slave << " - Set up as child...");

  // Closes PTM, the pseudoterminal multiplexer master (PTMX).
  (void)Master.release();

  CheckedErrnoThrow(
    [this] { return ::login_tty(Slave); }, "login_tty in child", -1);

  // Generally the PTY children are exec()ing away, so we can safely just NOT
  // set up the Pipe data structures here, right?
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
  CheckedErrnoThrow(
    [RawFD = Master.get(), &Size] { return ::ioctl(RawFD, TIOCSWINSZ, &Size); },
    "ioctl(PTMX, TIOCSWINSZ /* set window size*/);",
    -1);
}

} // namespace monomux::system::unix

#undef LOG_WITH_IDENTIFIER
#undef LOG
