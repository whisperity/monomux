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

#include <optional>

namespace monomux
{

/// Responsible for wrapping a low-level psuedo terminal teletypewriter (PTTY)
/// interface. A pseudoterminal is an emulation of the ancient technology where
/// physical typewriter and printer machines were connected to computers.
class Pty
{
  bool IsMaster = false;

public:
  fd Master, Slave;
  // std::array<fd, 2> Pipes;

public:
  Pty();

  /// \returns whether the current instance is open on the master (PTM, control)
  /// side.
  bool isMaster() const noexcept { return IsMaster; }

  /// \returns whether the current instance is open on the slave (PTS, process)
  /// side.
  bool isSlave() const noexcept { return !IsMaster; }

  /// \returns the raw file descriptor for the \p Pty side that is currently
  /// open.
  raw_fd getFD() const noexcept { return isMaster() ? Master : Slave; }

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
};

} // namespace monomux
