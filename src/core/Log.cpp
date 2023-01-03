/* SPDX-License-Identifier: LGPL-3.0-only */
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
                      << "Initialised at address" << ' ' << Singleton.get());
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
      LogPrefix << '[' << SN << ']' << ' ';
    if (!Facility.empty())
      LogPrefix << Facility;
    else
      LogPrefix << "<Unknown>";
    LogPrefix << ':' << ' ';
  }
  return OutputBuffer{*OS, Discarding, LogPrefix.str()};
}

} // namespace monomux::log
