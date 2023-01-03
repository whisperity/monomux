/* SPDX-License-Identifier: GPL-3.0-only */
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
