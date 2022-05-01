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
#include <cassert>
#include <memory>
#include <sstream>
#include <string_view>

#include "monomux/Config.h"
#include "monomux/Debug.h"

namespace monomux::log
{

/// Severity levels for log messages.
/// Severities linearly increase in "verbosity" level, so a lower severity
/// \e number indicate a higher severity of the message.
enum Severity
{
  /// The highest severity level. Will always be printed, no matter what.
  None = 0,
  /// Critical messages that are likely the last printout from the system.
  Fatal,
  /// Errors indicate operation failures which can be recovered from without
  /// exploding, but the operation itself cannot meaningfully continue.
  Error,
  /// Warnings indicate oopsies in operation which can be recovered from fully.
  Warning,
  /// The standard log level.
  Info,
  /// Debug information are meaningful only when trying to diagnose bogus
  /// behaviour or crashing.
  Debug,
  /// Verbose debug information that creates a printout at every important
  /// interaction. Only meaningful in debug conditions.
  Trace,
  /// The most verbose debug information which also prints raw data from the
  /// communication channels.
  Data,

  /// The default severity level for bare printouts when a severity is not
  /// specified.
  Default = Info,

  /// The largest severity value.
  Max = None,
  /// The lowest severity value.
  Min = Data,
};

/// The largest verbosity the user can request an increase to.
constexpr std::int8_t MaximumVerbosity = log::Min - log::Default;
/// The smallest verbosity (largest quietness) the user can decrease to.
constexpr std::int8_t MinimumVerbosity = log::Default - log::Max - 1;

/// The \p Logger class handles emitting log messages to an output device.
///
/// \note This object is \b NOT thread-safe!
class Logger
{
private:
  class OutputBuffer
  {
    bool Discard;
    std::ostream* OS;
    std::ostringstream Buffer;

  public:
    /// Wraps an output device into a log buffer.
    ///
    /// \param Discard Whether to throw the logged data away.
    OutputBuffer(std::ostream& OS, bool Discard, std::string_view Prefix);

    /// Print the contents of the log buffer to the output device.
    ~OutputBuffer() noexcept(false);

    /// Print the contents of the fed value to the internal buffer.
    template <typename T> OutputBuffer& operator<<(T&& Value)
    {
      if (!Discard)
        Buffer << std::forward<T>(Value);
      return *this;
    }
  };

  /// A global instance of the logger.
  static std::unique_ptr<Logger> Singleton;

public:
  /// \returns a human-readable tag for the specified severity.
  static const char* levelName(Severity S) noexcept;

  /// Retrieve the logging instance for the current application.
  static Logger& get();

  /// Retrieve the logging instance for the current application, if any was
  /// spawned. Otherwise, returns \p nullptr.
  static Logger* tryGet();

  /// Creates a new \p Logger object that has no connection with the global
  /// logging instance.
  ///
  /// \note In most of the cases, you do not want to do this.
  ///
  /// \see get()
  Logger(Severity SeverityLimit, std::ostream& OS);

  Severity getLimit() const noexcept { return SeverityLimit; }
  void setLimit(Severity Limit) noexcept { SeverityLimit = Limit; }

  /// Redirects all log messages after the call to this function to another
  /// output device.
  void setOutput(std::ostream& OS) noexcept { this->OS = &OS; }

  /// Starts printing a log message with the specified \p S severity.
  /// If the \p S severity is lower than the current severity limit, the message
  /// will be discarded.
  OutputBuffer operator()(Severity S, std::string_view Facility);

private:
  Severity SeverityLimit;
  std::ostream* OS;
};

#define MONOMUX_LOGGER_SHORTCUT(NAME, SEVERITY)                                \
  inline decltype(auto) NAME(std::string_view Facility)                        \
  {                                                                            \
    return monomux::log::Logger::get()(SEVERITY, Facility);                    \
  }

MONOMUX_LOGGER_SHORTCUT(always, None);
MONOMUX_LOGGER_SHORTCUT(log, Default);

MONOMUX_LOGGER_SHORTCUT(fatal, Fatal);
MONOMUX_LOGGER_SHORTCUT(error, Error);
MONOMUX_LOGGER_SHORTCUT(warn, Warning);
MONOMUX_LOGGER_SHORTCUT(info, Info);
MONOMUX_LOGGER_SHORTCUT(debug, Debug);
MONOMUX_LOGGER_SHORTCUT(trace, Trace);
MONOMUX_LOGGER_SHORTCUT(data, Data);

#undef MONOMUX_LOGGER_SHORTCUT

#ifdef MONOMUX_NON_ESSENTIAL_LOGS
/* Wrap logging code into this macro to suppress building it if config option
 * \p MONOMUX_NON_ESSENTIAL_LOGS is turned off.
 *
 * It has been turned \e ON in this build, and trace logging is compiled.
 */
#define MONOMUX_TRACE_LOG(X) MONOMUX_DETAIL_CONDITIONALLY_TRUE(X)
#else
/* Wrap logging code into this macro to suppress building it if config option
 * \p MONOMUX_NON_ESSENTIAL_LOGS is turned off.
 *
 * It has been turned \b OFF in this build, and trace logging is stripped.
 */
#define MONOMUX_TRACE_LOG(X) MONOMUX_DETAIL_CONDITIONALLY_FALSE(X)
#endif

} // namespace monomux::log
