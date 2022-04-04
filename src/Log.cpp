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

#include "monomux/system/Time.hpp"

#include "monomux/Log.hpp"

namespace monomux::log
{

static constexpr const char* SeverityName[Min + 1] = {"",
                                                      "!!! FATAL",
                                                      " !! ERROR",
                                                      "  ! WARNING",
                                                      "    INFO",
                                                      "  > DEBUG",
                                                      " >> TRACE",
                                                      ">>> DATA"};

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
    // FIXME: Use the default loglevel here and implement verbosity on the
    // commandline.
    Singleton = std::make_unique<Logger>(Min, std::clog);
    MONOMUX_TRACE_LOG(Singleton->operator()(log::Debug, "logger")
                      << "Logger initialised at address " << Singleton.get());
  }

  return *Singleton;
}

Logger* Logger::tryGet() { return Singleton.get(); }

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
