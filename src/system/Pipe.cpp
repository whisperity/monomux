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
#include <sys/stat.h>
#include <unistd.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/Pipe.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Pipe")

namespace monomux
{

static constexpr auto UserACL = S_IRUSR | S_IWUSR;

Pipe::Pipe(fd Handle, std::string Identifier, bool NeedsCleanup)
  : CommunicationChannel(std::move(Handle), std::move(Identifier), NeedsCleanup)
{}

Pipe Pipe::create(std::string Path, bool InheritInChild)
{
  CheckedPOSIXThrow(
    [&Path] { return ::mkfifo(Path.c_str(), UserACL); }, "mkfifo()", -1);

  fd::flag_t ExtraFlags = InheritInChild ? 0 : O_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [&Path, ExtraFlags] { return ::open(Path.c_str(), O_WRONLY | ExtraFlags); },
    "open('" + Path + "')",
    -1);

  LOG(debug) << "Created FIFO at " << Path;

  Pipe P{std::move(Handle), std::move(Path), true};
  P.OpenedAs = Write;
  return P;
}

Pipe::AnonymousPipe Pipe::create(bool InheritInChild)
{
  POD<raw_fd[2]> PipeFDs;
  fd::flag_t ExtraFlags = InheritInChild ? 0 : O_CLOEXEC;

  CheckedPOSIXThrow(
    [&PipeFDs, ExtraFlags] { return ::pipe2(PipeFDs, ExtraFlags); },
    "pipe2()",
    -1);

  AnonymousPipe PipePair;
  PipePair.Read = std::make_unique<Pipe>(Pipe::wrap(PipeFDs[0], Read, {}));
  PipePair.Write = std::make_unique<Pipe>(Pipe::wrap(PipeFDs[1], Write, {}));
  return PipePair;
}

Pipe Pipe::open(std::string Path, Mode OpenMode, bool InheritInChild)
{
  fd::flag_t ExtraFlags = InheritInChild ? 0 : O_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [&Path, OpenMode, ExtraFlags] {
      return ::open(Path.c_str(), OpenMode | ExtraFlags);
    },
    "open('" + Path + "')",
    -1);

  LOG(debug) << "Opened FIFO at " << Path << "for "
             << (OpenMode == Read ? "Read" : "Write");

  Pipe P{std::move(Handle), std::move(Path), false};
  P.OpenedAs = OpenMode;
  return P;
}

Pipe Pipe::wrap(fd&& FD, Mode OpenMode, std::string Identifier)
{
  if (Identifier.empty())
  {
    Identifier.push_back('<');
    if (OpenMode == Read)
      Identifier.push_back('r');
    else if (OpenMode == Write)
      Identifier.push_back('w');
    Identifier.append(":pipe-fd:");
    Identifier.append(std::to_string(FD.get()));
    Identifier.push_back('>');
  }

  LOG(trace) << "Pipeified FD " << Identifier;

  Pipe P{std::move(FD), std::move(Identifier), false};
  P.OpenedAs = OpenMode;
  return P;
}

Pipe::Pipe(Pipe&& RHS) noexcept
  : CommunicationChannel(std::move(RHS)), OpenedAs(std::move(RHS.OpenedAs)),
    Nonblock(std::move(RHS.Nonblock))
{}

Pipe& Pipe::operator=(Pipe&& RHS) noexcept
{
  if (this == &RHS)
    return *this;

  CommunicationChannel::operator=(std::move(RHS));

  OpenedAs = std::move(RHS.OpenedAs);
  Nonblock = std::move(RHS.Nonblock);

  return *this;
}

Pipe::~Pipe() noexcept
{
  if (needsCleanup())
  {
    auto RemoveResult =
      CheckedPOSIX([this] { return ::unlink(identifier().c_str()); }, -1);
    if (!RemoveResult)
    {
      LOG(error) << "Failed to remove file \"" << identifier()
                 << "\" when closing the pipe.\n\t" << RemoveResult.getError();
    }
  }
}

void Pipe::setBlocking()
{
  if (isBlocking())
    return;
  fd::removeStatusFlag(Handle.get(), O_NONBLOCK);
  Nonblock = false;
}

void Pipe::setNonblocking()
{
  if (isNonblocking())
    return;
  fd::addStatusFlag(Handle.get(), O_NONBLOCK);
  Nonblock = true;
}

std::string Pipe::read(raw_fd FD, std::size_t Bytes, bool* Success)
{
  std::string Return;
  Return.reserve(Bytes);

  std::size_t RemainingBytes = Bytes;

  bool ContinueReading = true;
  while (ContinueReading && RemainingBytes > 0 && RemainingBytes <= Bytes)
  {
    POD<char[BufferSize]> RawBuffer;
    auto ReadBytes = CheckedPOSIX(
      [FD, ReadSize = std::min(BufferSize, RemainingBytes), &RawBuffer] {
        return ::read(FD, &RawBuffer, ReadSize);
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
        break;
      }

      LOG(error) << FD << ": Read error";
      if (Success)
        *Success = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    if (ReadBytes.get() == 0)
    {
      LOG(error) << FD << ": Disconnected";
      ContinueReading = false;
      break;
    }

    std::size_t BytesToAppendFromCurrentRead =
      std::min(static_cast<std::size_t>(ReadBytes.get()), RemainingBytes);
    Return.append(RawBuffer, BytesToAppendFromCurrentRead);

    RemainingBytes -= BytesToAppendFromCurrentRead;
  }

  if (!ContinueReading && Return.empty() && Success)
    *Success = false;
  else if (Success)
    *Success = true;
  return Return;
}

std::size_t Pipe::write(raw_fd FD, std::string_view Buffer, bool* Success)
{
  std::size_t BytesSent = 0;
  while (!Buffer.empty())
  {
    auto SentBytes = CheckedPOSIX(
      [FD, WriteSize = std::min(BufferSize, Buffer.size()), &Buffer] {
        return ::write(FD, Buffer.data(), WriteSize);
      },
      -1);
    if (!SentBytes)
    {
      std::errc EC = static_cast<std::errc>(SentBytes.getError().value());
      if (EC == std::errc::interrupted /* EINTR */)
        // Not an error.
        continue;

      LOG(error) << FD << ": Write error";
      if (Success)
        *Success = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    if (SentBytes.get() == 0)
    {
      LOG(error) << FD << ": Disconnected";
      if (Success)
        *Success = false;
      break;
    }

    BytesSent += SentBytes.get();
    Buffer.remove_prefix(SentBytes.get());
  }

  if (Success)
    *Success = true;
  return BytesSent;
}

std::string Pipe::readImpl(std::size_t Bytes, bool& Continue)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Pipe failed."};
  if (OpenedAs != Read)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Not readable."};

  bool Success;
  std::string Data = Pipe::read(Handle, Bytes, &Success);
  if (!Success)
  {
    setFailed();
    Continue = false;
  }

  return Data;
}

std::size_t Pipe::writeImpl(std::string_view Buffer, bool& Continue)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Pipe failed."};
  if (OpenedAs != Write)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Not readable."};

  bool Success;
  std::size_t Bytes = Pipe::write(Handle, Buffer, &Success);
  if (!Success)
  {
    setFailed();
    Continue = false;
  }
  return Bytes;
}

std::unique_ptr<Pipe> Pipe::AnonymousPipe::takeRead()
{
  if (!Read)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Read end of pipe already taken."};
  if (Write)
    Write.reset();
  return std::move(Read);
}

std::unique_ptr<Pipe> Pipe::AnonymousPipe::takeWrite()
{
  if (!Write)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Write end of pipe already taken."};
  if (Read)
    Read.reset();
  return std::move(Write);
}

} // namespace monomux
