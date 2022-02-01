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
#include "Process.hpp"
#include "CheckedPOSIX.hpp"

#include <cstring>

namespace monomux {

static void allocCopyString(const std::string& Source,
                            char* DestinationStringArray[],
                            std::size_t Index)
{
  DestinationStringArray[Index] =
    reinterpret_cast<char*>(std::calloc(Source.size() + 1, 1));
  std::strncpy(
    DestinationStringArray[Index], Source.c_str(), Source.size() + 1);
}

[[noreturn]] void Process::exec(const Process::SpawnOptions& Opts)
{
  char** NewArgv = new char*[Opts.Arguments.size() + 2];
  allocCopyString(Opts.Program, NewArgv, 0);
  NewArgv[Opts.Arguments.size() + 1] = nullptr;
  for (std::size_t I = 0; I < Opts.Arguments.size(); ++I)
    allocCopyString(Opts.Arguments[I], NewArgv, I + 1);

  for (const auto& E : Opts.Environment)
  {
    if (!E.second.has_value())
    {
      CheckedPOSIX([&K = E.first] { return ::unsetenv(K.c_str()); }, -1);
    }
    else
    {
      CheckedPOSIX(
        [&K = E.first, &V = E.second] {
          return ::setenv(K.c_str(), V->c_str(), 1);
        },
        -1);
    }
  }

  CheckedPOSIXThrow(
    [&Opts, NewArgv] { return ::execvp(Opts.Program.c_str(), NewArgv); },
    "Executing process failed",
    -1);
  ::_Exit(EXIT_FAILURE); // [[noreturn]]
}

} // namespace monomux
