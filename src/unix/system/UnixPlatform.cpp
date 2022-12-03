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
#include <optional>

#include <libgen.h>
#include <linux/limits.h>
#include <sys/stat.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/system/Environment.hpp"

#include "monomux/system/UnixPlatform.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/UnixPlatform")

namespace monomux::system
{

std::string Platform::defaultShell()
{
  std::string EnvVar = getEnv("SHELL");
  if (!EnvVar.empty())
    return EnvVar;

  auto Check = [](const std::string& Program) {
    LOG(debug) << "Trying Shell program " << Program;
    return CheckedErrno(
      [Prog = Program.c_str()] {
        POD<struct ::stat> StatResult;
        return ::stat(Prog, &StatResult);
      },
      -1);
  };

  if (Check("/bin/bash"))
    return "/bin/bash";
  if (Check("/bin/sh"))
    return "/bin/sh";

  // Did not succeed.
  LOG(debug) << "No Shell found.";
  return {};
}

Platform::SocketPath Platform::SocketPath::defaultSocketPath()
{
  SocketPath R;

  std::string Dir = getEnv("XDG_RUNTIME_DIR");
  if (!Dir.empty())
  {
    LOG(debug) << "Socket path under XDG_RUNTIME_DIR";
    return {std::move(Dir), "mnmx", true};
  }

  Dir = getEnv("TMPDIR");
  if (!Dir.empty())
  {
    std::string User = getEnv("USER");
    if (!User.empty())
    {
      LOG(debug) << "Socket path under TMPDIR for $USER";
      return {std::move(Dir), "mnmx" + std::move(User), false};
    }
    LOG(debug) << "Socket path under TMPDIR";
    return {std::move(Dir), "mnmx", false};
  }

  LOG(debug) << "Socket path under hardcoded /tmp";
  return {"/tmp", "mnmx", false};
}

Platform::SocketPath Platform::SocketPath::absolutise(const std::string& Path)
{
  LOG(trace) << "Absolutising path \"" << Path << "\"...";

  POD<char[PATH_MAX]> Result;
  if (Path.front() == '/')
  {
    // If the path begins with a '/', assume it is absolute.
    LOG(trace) << '"' << Path << "\" is already absolute.";
    std::strncpy(Result, Path.c_str(), PATH_MAX);
  }
  else
  {
    auto R = CheckedErrno(
      [&Path, &Result] { return ::realpath(Path.c_str(), Result); }, nullptr);
    if (!R)
    {
      std::error_code EC = R.getError();
      LOG(trace) << "realpath(" << Path << ") failed: " << EC.message();
      if (EC == std::errc::no_such_file_or_directory /* ENOENT */)
      {
        // (For files that do not exist, realpath will fail. So we do the
        // absolute path conversion ourselves.)
        Result.reset();
        R.get() =
          CheckedErrnoThrow([&Result] { return ::realpath(".", Result); },
                            "realpath(\".\")",
                            nullptr);
        LOG(trace) << "realpath(.) = " << R.get();

        std::size_t ReadlinkCurDirPathSize = std::strlen(Result);
        if (ReadlinkCurDirPathSize + 1 + Path.size() > PATH_MAX)
          throw std::system_error{
            std::make_error_code(std::errc::filename_too_long), "strncat path"};

        Result[ReadlinkCurDirPathSize] = '/';
        Result[ReadlinkCurDirPathSize + 1] = 0;
        std::strncat(
          Result + ReadlinkCurDirPathSize, Path.c_str(), Path.size());
        LOG(trace) << "realpath(.) + " << Path << " -> " << Result;
      }
      else
        throw std::system_error{EC, "realpath()"};
    }
    assert(R.get() == &Result[0]);
  }

  POD<char[PATH_MAX]> Dir;
  std::strncpy(Dir, Result, PATH_MAX);
  char* DirResult =
    CheckedErrnoThrow([&Dir] { return ::dirname(Dir); }, "dirname()", nullptr);
  char* BaseResult = CheckedErrnoThrow(
    [&Result] { return ::basename(Result); }, "basename()", nullptr);

  LOG(trace) << "Path split: dirname = " << DirResult
             << "; name = " << BaseResult;

  SocketPath SP;
  SP.Path = DirResult;
  SP.Filename = BaseResult;
  return SP;
}

std::string Platform::SocketPath::to_string() const
{
  std::ostringstream Buf;
  if (!Path.empty())
    Buf << Path << '/';
  Buf << Filename;
  return Buf.str();
}

} // namespace monomux::system

#undef LOG
