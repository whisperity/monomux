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
#include <iostream>

#include "monomux/Time.hpp"

#include "monomux/Log.hpp"

namespace monomux::log
{

// clang-format off
static constexpr const char* SeverityName[Min + 1] = {"           ",
                                                      "!!! FATAL  ",
                                                      " !! ERROR  ",
                                                      "  ! Warning",
                                                      "    Info   ",
                                                      "  > Debug  ",
                                                      " >> trace  ",
                                                      ">>> data   "};
static constexpr const char InvalidSeverity[] =       "??? Invalid";
// clang-format on

const char* Logger::levelName(Severity S) noexcept
{
  if (S > log::Min || S < log::Max)
  {
    assert(false && "Invalid severity to stringify!");
    return InvalidSeverity;
  }
  return SeverityName[S];
}

Logger::OutputBuffer::OutputBuffer(std::ostream& OS,
                                   bool Discard,
                                   std::string_view Prefix)
  : Discard(Discard), OS(&OS)
{
  if (!Discard)
    Buffer << Prefix;
}

Logger::OutputBuffer::~OutputBuffer() noexcept(false)
{
  if (!Discard)
    (*OS) << Buffer.str() << std::endl;
}

std::unique_ptr<Logger> Logger::Singleton;

Logger& Logger::get()
{
  if (!Singleton)
  {
    Singleton = std::make_unique<Logger>(Default, std::clog);
    MONOMUX_TRACE_LOG(Singleton->operator()(log::Debug, "logger")
                      << "Initialised at address " << Singleton.get());
  }

  return *Singleton;
}

Logger* Logger::tryGet() { return Singleton.get(); }

std::size_t Logger::digits(std::size_t Number)
{
  std::size_t R = 1;
  while (Number > 0)
  {
    Number /= 10; // NOLINT(readability-magic-numbers)
    ++R;
  }
  return R;
}

Logger::Logger(Severity S, std::ostream& OS) : SeverityLimit(S), OS(&OS) {}

Logger::OutputBuffer Logger::operator()(Severity S, std::string_view Facility)
{
  std::ostringstream LogPrefix;
  bool Discarding = S > getLimit();
  if (!Discarding)
  {
    LogPrefix << '[' << formatTime(std::chrono::system_clock::now()) << ']';
    if (std::string_view SN = SeverityName[S]; !SN.empty())
      LogPrefix << '[' << SN << "] ";
    if (!Facility.empty())
      LogPrefix << Facility << ": ";
    else
      LogPrefix << "?: ";
  }
  return OutputBuffer{*OS, Discarding, LogPrefix.str()};
}

} // namespace monomux::log
