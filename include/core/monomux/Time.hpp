/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace monomux
{

/// Formats the given \p Chrono \p Time object to an internationally viable
/// representation.
template <typename T>[[nodiscard]] std::string formatTime(const T& Time)
{
  std::time_t RawTime = T::clock::to_time_t(Time);
  std::tm SplitTime = *std::localtime(&RawTime);

  std::ostringstream Buf;
  // Mirror the behaviour of tmux/byobu menu.
  Buf << std::put_time(&SplitTime, "%a %b %e %H:%M:%S %Y");
  return Buf.str();
}

} // namespace monomux
