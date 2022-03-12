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
#include "Pipe.hpp"

#include "CheckedPOSIX.hpp"
#include "POD.hpp"

#include <iostream>

// #include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace monomux
{

static constexpr auto UserACL = S_IRUSR | S_IWUSR;

Pipe Pipe::create(std::string Path, bool InheritInChild)
{
  CheckedPOSIXThrow(
    [&Path] { return ::mkfifo(Path.c_str(), UserACL); }, "mkfifo()", -1);

  flag_t ExtraFlags = InheritInChild ? 0 : O_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [&Path, ExtraFlags] { return ::open(Path.c_str(), O_WRONLY | ExtraFlags); },
    "open('" + Path + "')",
    -1);

  Pipe P;
  P.Handle = std::move(Handle);
  P.OpenedAs = Write;
  P.Path = std::move(Path);
  P.Owning = true;
  P.CleanupPossible = true;
  return P;
}

Pipe Pipe::open(std::string Path, Mode OpenMode, bool InheritInChild)
{
  flag_t ExtraFlags = InheritInChild ? 0 : O_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [&Path, OpenMode, ExtraFlags] {
      return ::open(Path.c_str(), OpenMode | ExtraFlags);
    },
    "open('" + Path + "')",
    -1);

  Pipe P;
  P.Handle = std::move(Handle);
  P.OpenedAs = OpenMode;
  P.Path = std::move(Path);
  P.Owning = false;
  P.CleanupPossible = false;
  return P;
}

Pipe Pipe::wrap(fd&& FD, Mode OpenMode)
{
  Pipe P;
  P.Path = "<fd:";
  P.Path.append(std::to_string(FD.get()));
  P.Path.push_back(':');
  P.Path.append(OpenMode == Read ? "r" : "w");
  P.Path.push_back('>');
  P.Handle = std::move(FD);
  P.OpenedAs = OpenMode;
  P.Owning = true;
  P.CleanupPossible = false;
  return P;
}

Pipe::Pipe(Pipe&& RHS) noexcept
  : Handle(std::move(RHS.Handle)), OpenedAs(RHS.OpenedAs),
    Path(std::move(RHS.Path)), Owning(RHS.Owning),
    CleanupPossible(RHS.CleanupPossible), Open(RHS.Open)
{}

Pipe& Pipe::operator=(Pipe&& RHS) noexcept
{
  if (this == &RHS)
    return *this;

  Handle = std::move(RHS.Handle);
  OpenedAs = RHS.OpenedAs;
  Path = std::move(RHS.Path);
  Owning = RHS.Owning;
  CleanupPossible = RHS.CleanupPossible;

  return *this;
}

Pipe::~Pipe() noexcept
{
  if (!Handle.has())
    return;

  if (Owning && CleanupPossible)
  {
    auto RemoveResult =
      CheckedPOSIX([this] { return ::unlink(Path.c_str()); }, -1);
    if (!RemoveResult)
    {
      std::cerr << "Failed to remove file '" << Path
                << "' when closing the pipe.\n";
      std::cerr << std::strerror(RemoveResult.getError().value()) << std::endl;
    }
  }

  CheckedPOSIX([this] { return ::close(Handle); }, -1);
}

void Pipe::setBlocking()
{
  if (isBlocking())
    return;
  fd::removeStatusFlag(Handle.get(), O_NONBLOCK);
}

void Pipe::setNonblocking()
{
  if (isNonblocking())
    return;
  fd::addStatusFlag(Handle.get(), O_NONBLOCK);
}

std::string Pipe::read(fd& FD, std::size_t Bytes, bool* Success)
{

  // NOLINTNEXTLINE(readability-identifier-naming)
  static constexpr std::size_t BUFFER_SIZE = 1024;

  std::string Return;
  Return.reserve(Bytes);
  POD<char[BUFFER_SIZE]> Buffer;

  while (Return.size() < Bytes)
  {
    auto ReadBytes = CheckedPOSIX(
      [RawFD = FD.get(), &Buffer] {
        Buffer.reset();

        return ::read(RawFD, &Buffer, BUFFER_SIZE);
      },
      -1);
    if (!ReadBytes)
    {
      std::errc EC = static_cast<std::errc>(ReadBytes.getError().value());
      if (EC == std::errc::interrupted /* EINTR */)
        // Not an error, continue.
        continue;
      if (EC == std::errc::operation_would_block /* EWOULDBLOCK */ ||
          EC == std::errc::resource_unavailable_try_again /* EAGAIN */)
      {
        // No more data left in the stream.
        std::cout << "read(): " << std::strerror(static_cast<int>(EC))
                  << std::endl;
        break;
      }

      std::cerr << "Pipe " << FD.get() << " - read error." << std::endl;
      if (Success)
        *Success = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    std::cout << "Received chunk " << ReadBytes.get() << " bytes from "
              << FD.get() << std::endl;
    if (ReadBytes.get() == 0)
    {
      std::cout << "Pipe " << FD.get() << " disconnected." << std::endl;
      if (Success)
        *Success = false;
      break;
    }

    if (Return.size() + ReadBytes.get() <= Bytes)
    {
      std::cout << "Space remaining, current=" << Return.size()
                << ", inserting " << ReadBytes.get() << " bytes..."
                << std::endl;
      Return.insert(Return.size(), *Buffer, ReadBytes.get());
      std::cout << "Now data at " << Return.size() << ", continuing..."
                << std::endl;
    }
    else
    {
      // Do not overflow the requested amount.
      std::cout << "Input would overflow, appending only "
                << Bytes - Return.size() << " bytes." << std::endl;
      Return.insert(Return.size(), *Buffer, Bytes - Return.size());
      break;
    }
  }

  std::cout << "Finished reading " << Return.size() << " bytes." << std::endl;

  if (Success)
    *Success = true;
  return Return;
}

std::string Pipe::read(std::size_t Bytes)
{
  if (!Open)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Closed."};
  if (OpenedAs != Read)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Not readable."};

  bool Success;
  std::string Data = read(Handle, Bytes, &Success);
  if (!Success)
    Open = false;

  return Data;
}

void Pipe::write(fd& FD, std::string_view Data, bool* Success)
{
  // NOLINTNEXTLINE(readability-identifier-naming)
  static constexpr std::size_t BUFFER_SIZE = 1024;

  const std::size_t DataSize = Data.size();

  std::clog << "DEBUG: Writing data to pipe:\n\t" << Data << std::endl;
  while (!Data.empty())
  {
    auto SentBytes = CheckedPOSIX(
      [RawFD = FD.get(), &Data] {
        return ::write(RawFD, Data.data(), std::min(BUFFER_SIZE, Data.size()));
      },
      -1);
    if (!SentBytes)
    {
      std::errc EC = static_cast<std::errc>(SentBytes.getError().value());
      if (EC == std::errc::interrupted /* EINTR */)
        // Not an error.
        continue;

      std::cerr << "Pipe " << FD.get() << " - write error." << std::endl;
      if (Success)
        *Success = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    std::cout << "Sent " << SentBytes.get() << " bytes to " << FD.get()
              << std::endl;
    if (SentBytes.get() == 0)
    {
      std::cout << "Pipe " << FD.get() << " disconnected." << std::endl;
      if (Success)
        *Success = false;
      break;
    }

    if (static_cast<std::size_t>(SentBytes.get()) <= Data.size())
      Data.remove_prefix(SentBytes.get());
    else
      Data.remove_prefix(Data.size());
  }

  std::cout << "Finished writing " << DataSize << " bytes." << std::endl;
  if (Success)
    *Success = true;
}

void Pipe::write(std::string_view Data)
{
  if (!Open)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Closed."};
  if (OpenedAs != Write)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Not writable."};

  bool Success;
  write(Handle, Data, &Success);
  if (!Success)
    Open = false;
}

} // namespace monomux
