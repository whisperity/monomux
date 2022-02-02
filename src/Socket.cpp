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
#include "Socket.hpp"
#include "CheckedPOSIX.hpp"
#include "POD.hpp"

#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace monomux
{

Socket Socket::create(std::string Path, bool InheritInChild)
{
  flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "Error creating socket '" + Path + "'",
    -1);

  // TODO: Mask?

  POD<struct ::sockaddr_un> SocketAddr;
  SocketAddr->sun_family = AF_UNIX;
  std::strncpy(
    SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

  CheckedPOSIXThrow(
    [&Handle, &SocketAddr] {
      return ::bind(Handle,
                    reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                    sizeof(SocketAddr));
    },
    "Failed to bind '" + Path + "'",
    -1);

  Socket S;
  S.Handle = std::move(Handle);
  S.Path = std::move(Path);
  S.Owning = true;
  S.CleanupPossible = true;
  return S;
}

Socket Socket::open(std::string Path, bool InheritInChild)
{
  flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "Error creating temporary client socket",
    -1);

  POD<struct ::sockaddr_un> SocketAddr;
  SocketAddr->sun_family = AF_UNIX;
  std::strncpy(
    SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

  CheckedPOSIXThrow(
    [&Handle, &SocketAddr] {
      return ::connect(Handle,
                       reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                       sizeof(SocketAddr));
    },
    "Failed to connect to '" + Path + "'",
    -1);

  Socket S;
  S.Handle = std::move(Handle);
  S.Path = std::move(Path);
  S.Owning = false;
  S.CleanupPossible = false;
  return S;
}

Socket Socket::wrap(fd&& FD)
{
  Socket S;
  S.Path = "<fd:";
  S.Path.append(std::to_string(FD.get()));
  S.Path.push_back('>');
  S.Handle = std::move(FD);
  S.Owning = true;
  S.CleanupPossible = false;
  return S;
}

Socket::Socket(Socket&& RHS)
  : Handle(std::move(RHS.Handle)), Path(std::move(RHS.Path)),
    Owning(RHS.Owning), CleanupPossible(RHS.CleanupPossible), Open(RHS.Open),
    Listening(RHS.Listening)
{}

Socket& Socket::operator=(Socket&& RHS)
{
  if (this == &RHS)
    return *this;

  Handle = std::move(RHS.Handle);
  Path = std::move(RHS.Path);
  Owning = RHS.Owning;
  CleanupPossible = RHS.CleanupPossible;
  Listening = RHS.Listening;

  return *this;
}

Socket::~Socket()
{
  if (!Handle.has())
    return;

  if (Owning && CleanupPossible)
  {
    auto RemoveResult =
      CheckedPOSIX([this] { return ::unlink(Path.c_str()); }, -1);
    if (!RemoveResult)
    {
      std::cerr << "Failed to remove '" << Path
                << "' when closing the socket.\n";
      std::cerr << std::strerror(RemoveResult.getError().value()) << std::endl;
    }
  }

  CheckedPOSIX([this] { return ::close(Handle); }, -1);
}

void Socket::listen()
{
  if (!Owning)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Can't start listening on an outbound socket!"};

  if (Listening)
    throw std::system_error{
      std::make_error_code(std::errc::operation_in_progress),
      "The socket is already listening!"};

  CheckedPOSIXThrow([this] { return ::listen(Handle, 128); }, "listen()", -1);
  Listening = true;
}

std::string Socket::read(std::size_t Bytes)
{
  if (!Open)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Closed."};

  std::string Return;
  Return.reserve(Bytes);
  static constexpr std::size_t BUFFER_SIZE = 1024;
  POD<char[BUFFER_SIZE]> Buffer;

  while (Return.size() < Bytes)
  {
    auto ReadBytes = CheckedPOSIX(
      [this, &Buffer] { return ::recv(Handle.get(), &Buffer, BUFFER_SIZE, 0); },
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

      std::cerr << "Socket " << Handle.get() << " - read error." << std::endl;
      Open = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    std::cout << "Received chunk " << ReadBytes.get() << " bytes from "
              << Handle.get() << std::endl;
    if (ReadBytes.get() == 0)
    {
      std::cout << "Socket " << Handle.get() << " disconnected." << std::endl;
      Open = false;
      break;
    }
    // std::cout << "Valid input on socket " << Handle.get() << "\n"
    //           << std::string_view{*Buffer, BUFFER_SIZE} << std::endl;

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

  return Return;
}

void Socket::write(std::string Data)
{
  if (!Open)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Closed."};
  static constexpr std::size_t BUFFER_SIZE = 1024;

  std::string_view SV{Data.data(), Data.size()};
  while (!SV.empty())
  {
    auto SentBytes = CheckedPOSIX(
      [this, &SV] {
        return ::send(
          Handle.get(), SV.data(), std::min(BUFFER_SIZE, SV.size()), 0);
      },
      -1);
    if (!SentBytes)
    {
      std::errc EC = static_cast<std::errc>(SentBytes.getError().value());
      if (EC == std::errc::interrupted /* EINTR */)
        // Not an error.
        continue;

      std::cerr << "Socket " << Handle.get() << " - write error." << std::endl;
      Open = false;
      throw std::system_error{std::make_error_code(EC)};
    }

    std::cout << "Sent " << SentBytes.get() << " bytes to " << Handle.get()
              << std::endl;
    if (SentBytes.get() == 0)
    {
      std::cout << "Socket " << Handle.get() << " disconnected." << std::endl;
      Open = false;
      break;
    }

    if (SentBytes.get() <= SV.size())
      SV.remove_prefix(SentBytes.get());
    else
      SV.remove_prefix(SV.size());
  }

  std::cout << "Finished sending " << Data.size() << " bytes." << std::endl;
}

} // namespace monomux
