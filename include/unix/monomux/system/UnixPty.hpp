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

#include "monomux/system/Pty.hpp"

namespace monomux::system::unix
{

/// Responsible for wrapping a low-level psuedo terminal teletypewriter (PTTY)
/// interface. A pseudoterminal is an emulation of the ancient technology where
/// physical typewriter and printer machines were connected to computers.
///
/// \see pty(7)
/// \see openpty(3)
class Pty : public system::Pty
{
public:
  /// Creates a new PTY-pair.
  Pty();

  void setupParentSide() override;

  void setupChildrenSide() override;

  void setSize(unsigned short Rows, unsigned short Columns) override;
};

} // namespace monomux::system::unix
