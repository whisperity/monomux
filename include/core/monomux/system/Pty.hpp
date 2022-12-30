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
#include <optional>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/Pipe.hpp"

namespace monomux::system
{

/// Responsible for wrapping a low-level psuedo terminal teletypewriter (PTTY)
/// interface. A pseudoterminal is an emulation of the ancient technology where
/// physical typewriter and printer machines were connected to computers.
class Pty
{
protected:
  UniqueScalar<bool, false> IsMaster;
  Handle Master;
  Handle Slave;
  std::string Name;

  /// A \p Pipe for reading from the other side. Only established \b AFTER
  /// setting up either as parent or childside.
  std::unique_ptr<Pipe> Read;
  /// A \p Pipe for writing to the other side. Only established \b AFTER
  /// setting up either as parent or childside.
  std::unique_ptr<Pipe> Write;

  Pty() = default;

public:
  virtual ~Pty() = default;

  /// \returns whether the current instance is open on the master (PTM, control)
  /// side.
  [[nodiscard]] bool isMaster() const noexcept { return IsMaster; }

  /// \returns whether the current instance is open on the slave (PTS, process)
  /// side.
  [[nodiscard]] bool isSlave() const noexcept { return !IsMaster; }

  /// \returns the raw file descriptor for the \p Pty side that is currently
  /// open.
  [[nodiscard]] Handle& raw() noexcept { return isMaster() ? Master : Slave; }

  /// \returns the name of the PTY interface that was created (e.g. /dev/pts/2).
  [[nodiscard]] const std::string& name() const noexcept { return Name; }

  /// Returns the \p Pipe that can read from the standard output of the other
  /// end of the PTY.
  ///
  /// \note Using this method is only valid once the PTY has established its
  /// master/slave status.
  [[nodiscard]] Pipe& reader() noexcept
  {
    assert(Read);
    return *Read;
  }
  /// Returns the \p Pipe that can write data into the standard input of the
  /// other end of the PTY.
  ///
  /// \note Using this method is only valid once the PTY has established its
  /// master/slave status.
  [[nodiscard]] Pipe& writer() noexcept
  {
    assert(Write);
    return *Write;
  }

  /// Executes actions that configure the current PTY from the owning parent's
  /// point of view. This usually means that the PTS (pseudoterminal-slave)
  /// file descriptor is closed.
  virtual void setupParentSide() = 0;

  /// Executes actions that configure the current PTY from a running child
  /// process's standpoint, turning it into the controlling terminal of a
  /// process. The master file descriptor is also closed.
  ///
  /// Normally, after a call to this function, it is expected for the child
  /// process to be replaced with another one.
  virtual void setupChildrenSide() = 0;

  /// Sets the size of the pseudoterminal device to have the given dimensions.
  virtual void setSize(unsigned short Rows, unsigned short Columns) = 0;
};

} // namespace monomux::system
