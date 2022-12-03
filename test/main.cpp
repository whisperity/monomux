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
#include <csignal>
#include <iostream>

#include <gtest/gtest.h>

#include "monomux/system/Backtrace.hpp"

void stackTrace(int Signal)
{
  // Remove this handler and make the OS handle once we return.
  (void)std::signal(Signal, SIG_DFL);

  std::cerr << "FATAL! " << Signal << '\n';
  monomux::system::printBacktrace(std::cerr);
  std::cerr << std::endl;
}

int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);

  (void)std::signal(SIGABRT, stackTrace);
  (void)std::signal(SIGSEGV, stackTrace);

  return RUN_ALL_TESTS();
}
