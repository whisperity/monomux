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
#include "Environment.hpp"
#include "CheckedPOSIX.hpp"
#include "POD.hpp"

#include <cassert>
#include <climits>
#include <cstdlib>
#include <sstream>

#include <libgen.h>
#include <sys/stat.h>

namespace monomux
{

std::string getEnv(const std::string& Key)
{
  const char* const Value = std::getenv(Key.c_str());
  if (!Value)
    return {};
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

  // Try to see if /bin/bash is available and default to that.
  auto Bash = CheckedPOSIX(
    [] {
      POD<struct ::stat> Stat;
      return ::stat("/bin/bash", &Stat);
    },
    -1);
  if (Bash)
    return "/bin/bash";

  // Try to see if /bin/sh is available and default to that.
  auto Sh = CheckedPOSIX(
    [] {
      POD<struct ::stat> Stat;
      return ::stat("/bin/sh", &Stat);
    },
    -1);
  if (Sh)
    return "/bin/sh";

  // Did not succeed.
  return {};
}

SocketPath SocketPath::defaultSocketPath()
{
  SocketPath R;

  std::string Dir = getEnv("XDG_RUNTIME_DIR");
  if (!Dir.empty())
    return {std::move(Dir), "mnmx", true};

  Dir = getEnv("TMPDIR");
  if (!Dir.empty())
  {
    std::string User = getEnv("USER");
    if (!User.empty())
      return {std::move(Dir), "mnmx" + std::move(User), false};
    return {std::move(Dir), "mnmx", false};
  }

  return {"/tmp", "mnmx", false};
}

SocketPath SocketPath::absolutise(const std::string& Path)
{
  POD<char[PATH_MAX]> Result;
  if (Path.front() == '/')
    // If the path begins with a '/', assume it is absolute.
    std::strncpy(Result, Path.c_str(), PATH_MAX);
  else
  {
    auto R = CheckedPOSIX(
      [&Path, &Result] { return ::realpath(Path.c_str(), Result); }, nullptr);
    if (!R)
    {
      std::error_code EC = R.getError();
      if (EC == std::errc::no_such_file_or_directory /* ENOENT */)
      {
        // (For files that do not exist, realpath will fail. So we do the
        // absolute path conversion ourselves.)
        Result.reset();
        R.get() =
          CheckedPOSIXThrow([&Result] { return ::realpath(".", Result); },
                            "realpath(\".\")",
                            nullptr);

        std::size_t ReadlinkCurDirPathSize = std::strlen(Result);
        if (ReadlinkCurDirPathSize + 1 + Path.size() > PATH_MAX)
          throw std::system_error{
            std::make_error_code(std::errc::filename_too_long), "strncat path"};

        Result[ReadlinkCurDirPathSize] = '/';
        Result[ReadlinkCurDirPathSize + 1] = 0;
        std::strncat(
          Result + ReadlinkCurDirPathSize, Path.c_str(), Path.size());
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

  SocketPath SP;
  SP.Path = DirResult;
  SP.Filename = BaseResult;
  return SP;
}

std::string SocketPath::toString() const
{
  std::ostringstream Buf;
  Buf << Path << '/' << Filename;
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

  MonomuxSession S;
  S.Socket.Filename = SocketPath;
  S.SessionName = std::move(SessionName);
  return S;
}

} // namespace monomux
