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
#include "fd.hpp"

#include "adt/unique_scalar.hpp"

#include <optional>

namespace monomux
{

/// Responsible for wrapping a low-level psuedo terminal teletypewriter (PTTY)
/// interface. A pseudoterminal is an emulation of the ancient technology where
/// physical typewriter and printer machines were connected to computers.
class Pty
{
  unique_scalar<bool, false> IsMaster;
  fd Master;
  fd Slave;
  std::string Name;

public:
  /// Creates a new PTY-pair.
  Pty();

  /// \returns whether the current instance is open on the master (PTM, control)
  /// side.
  bool isMaster() const noexcept { return IsMaster; }

  /// \returns whether the current instance is open on the slave (PTS, process)
  /// side.
  bool isSlave() const noexcept { return !IsMaster; }

  /// \returns the raw file descriptor for the \p Pty side that is currently
  /// open.
  fd& raw() noexcept { return isMaster() ? Master : Slave; }

  /// \returns the name of the PTY interface that was created (e.g. /dev/pts/2).
  const std::string& name() const noexcept { return Name; }

  /// Executes actions that configure the current PTY from the owning parent's
  /// point of view. This usually means that the PTS (pseudoterminal-slave)
  /// file descriptor is closed.
  void setupParentSide();

  /// Executes actions that configure the current PTY from a running child
  /// process's standpoint, turning it into the controlling terminal of a
  /// process. The master file descriptor is also closed.
  ///
  /// Normally, after a call to this function, it is expected for the child
  /// process to be replaced with another one.
  void setupChildrenSide();

  /// Sets the size of the pseudoterminal device to have the given dimensions.
  void setSize(unsigned short Rows, unsigned short Columns);
};

} // namespace monomux
