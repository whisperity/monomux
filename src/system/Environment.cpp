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
#include <cassert>
#include <climits>
#include <cstdlib>
#include <sstream>

#include <libgen.h>
#include <sys/stat.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/Environment.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Environment")

namespace monomux
{

std::string getEnv(const std::string& Key)
{
  const char* const Value = std::getenv(Key.c_str());
  if (!Value)
  {
    MONOMUX_TRACE_LOG(LOG(data) << "getEnv(" << Key << ") -> unset");
    return {};
  }
  MONOMUX_TRACE_LOG(LOG(data) << "getEnv(" << Key << ") = " << Value);
  return {Value};
}

std::pair<std::string, std::string>
makeCurrentMonomuxSessionName(const std::string& SessionName)
{
  return {"MONOMUX_SESSION", SessionName};
}

std::optional<std::string> getCurrentMonomuxSessionName()
{
  std::string EnvVal = getEnv("MONOMUX_SESSION");
  if (EnvVal.empty())
    return std::nullopt;
  return EnvVal;
}

std::string defaultShell()
{
  std::string EnvVar = getEnv("SHELL");
  if (!EnvVar.empty())
    return EnvVar;

  auto Check = [](const std::string& Program) {
    LOG(debug) << "Trying Shell program " << Program;
    return CheckedPOSIX(
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

SocketPath SocketPath::defaultSocketPath()
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

SocketPath SocketPath::absolutise(const std::string& Path)
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
    auto R = CheckedPOSIX(
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
          CheckedPOSIXThrow([&Result] { return ::realpath(".", Result); },
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
    CheckedPOSIXThrow([&Dir] { return ::dirname(Dir); }, "dirname()", nullptr);
  char* BaseResult = CheckedPOSIXThrow(
    [&Result] { return ::basename(Result); }, "basename()", nullptr);

  LOG(trace) << "Path split: dirname = " << DirResult
             << "; name = " << BaseResult;

  SocketPath SP;
  SP.Path = DirResult;
  SP.Filename = BaseResult;
  return SP;
}

std::string SocketPath::toString() const
{
  std::ostringstream Buf;
  if (!Path.empty())
    Buf << Path << '/';
  Buf << Filename;
  return Buf.str();
}

std::vector<std::pair<std::string, std::string>>
MonomuxSession::createEnvVars() const
{
  std::vector<std::pair<std::string, std::string>> R;
  R.emplace_back(std::make_pair("MONOMUX_SOCKET", Socket.toString()));
  R.emplace_back(std::make_pair("MONOMUX_SESSION", SessionName));
  return R;
}

std::optional<MonomuxSession> MonomuxSession::loadFromEnv()
{
  std::string SocketPath = getEnv("MONOMUX_SOCKET");
  std::string SessionName = getEnv("MONOMUX_SESSION");

  if (SocketPath.empty() || SessionName.empty())
    return std::nullopt;

  LOG(data) << "Session from environment:\n\tServer socket: " << SocketPath
            << "\n\tSession name: " << SessionName;

  MonomuxSession S;
  S.Socket.Filename = SocketPath;
  S.SessionName = std::move(SessionName);
  return S;
}

} // namespace monomux
