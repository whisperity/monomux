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

namespace monomux
{

/// Contains the exit codes MonoMux \p main() functions return with.
enum ExitCode : int
{
  /// Successful execution (processes exited gracefully).
  EXIT_Success = 0,

  /// Indicates a fatal error in the communication with the server.
  EXIT_SystemError = 1,

  /// Values specified on the command-line of MonoMux are erroneous.
  EXIT_InvocationError = 2,
};

} // namespace monomux
