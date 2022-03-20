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

Socket::Socket(fd Handle, std::string Identifier, bool NeedsCleanup)
  : CommunicationChannel(std::move(Handle), std::move(Identifier), NeedsCleanup)
{}

Socket Socket::create(std::string Path, bool InheritInChild)
{
  fd::flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "socket()",
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
    "bind('" + Path + "')",
    -1);

  Socket S{std::move(Handle), std::move(Path), true};
  S.Owning = true;
  return S;
}

Socket Socket::connect(std::string Path, bool InheritInChild)
{
  fd::flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd Handle = CheckedPOSIXThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "socket()",
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
    "connect('" + Path + "')",
    -1);

  Socket S{std::move(Handle), std::move(Path), false};
  S.Owning = false;
  return S;
}

Socket Socket::wrap(fd&& FD, std::string Identifier)
{
  if (Identifier.empty())
  {
    Identifier.append("<sock-fd:");
    Identifier.append(std::to_string(FD.get()));
    Identifier.push_back('>');
  }

  Socket S{std::move(FD), std::move(Identifier), false};
  S.Owning = false;
  return S;
}

Socket::Socket(Socket&& RHS) noexcept
  : CommunicationChannel(std::move(RHS)), Owning(std::move(RHS.Owning)),
    Listening(std::move(RHS.Listening))
{}

Socket& Socket::operator=(Socket&& RHS) noexcept
{
  if (this == &RHS)
    return *this;

  CommunicationChannel::operator=(std::move(RHS));

  Owning = std::move(RHS.Owning);
  Listening = std::move(RHS.Listening);

  return *this;
}

Socket::~Socket() noexcept
{
  if (needsCleanup())
  {
    auto RemoveResult =
      CheckedPOSIX([this] { return ::unlink(identifier().c_str()); }, -1);
    if (!RemoveResult)
    {
      std::cerr << "Failed to remove file '" << identifier()
                << "' when closing the socket.\n";
      std::cerr << std::strerror(RemoveResult.getError().value()) << std::endl;
    }
  }
}

void Socket::listen(std::size_t QueueSize)
{
  if (!Owning)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Can't start listening on a non-controlled socket!"};
  if (Listening)
    throw std::system_error{
      std::make_error_code(std::errc::operation_in_progress),
      "The socket is already listening!"};

  CheckedPOSIXThrow(
    [this, QueueSize] { return ::listen(Handle, static_cast<int>(QueueSize)); },
    "listen()",
    -1);
  Listening = true;
}

std::optional<Socket> Socket::accept(std::error_code* Error, bool* Recoverable)
{
  POD<struct ::sockaddr_un> SocketAddr;
  POD<::socklen_t> SocketAddrLen;

  auto MaybeClient = CheckedPOSIX(
    [this, &SocketAddr, &SocketAddrLen] {
      return ::accept(raw(),
                      reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                      &SocketAddrLen);
    },
    -1);
  if (!MaybeClient)
  {
    if (Error)
      *Error = MaybeClient.getError();

    bool ConsiderRecoverable = false;
    std::errc EC = static_cast<std::errc>(MaybeClient.getError().value());
    if (EC == std::errc::resource_unavailable_try_again /* EAGAIN */ ||
        EC == std::errc::interrupted /* EINTR */ ||
        EC == std::errc::connection_aborted /* ECONNABORTED */)
    {
      std::cerr << "accept() " << std::make_error_code(EC) << std::endl;
      ConsiderRecoverable = false;
    }
    else if (EC == std::errc::too_many_files_open /* EMFILE */ ||
             EC == std::errc::too_many_files_open_in_system /* ENFILE */)
    {
      std::cerr << "accept() " << std::make_error_code(EC) << ", recoverable..."
                << std::endl;
      ConsiderRecoverable = true;
    }
    else
    {
      std::cerr << "accept() failed: " << MaybeClient.getError().message()
                << std::endl;
      ConsiderRecoverable = false;
    }

    if (Recoverable)
      *Recoverable = ConsiderRecoverable;
    return std::nullopt;
  }

  // Successfully accepted a client.
  return Socket::wrap(MaybeClient.get(), std::string{SocketAddr->sun_path});
}

std::string Socket::readImpl(std::size_t Bytes, bool& Continue)
{
  std::string Return;
  Return.reserve(Bytes);

  POD<char[BufferSize]> RawBuffer; // NOLINT(modernize-avoid-c-arrays)
  if (Bytes > BufferSize)
    Bytes = BufferSize;

  auto ReadBytes = CheckedPOSIX(
    [FD = Handle.get(), Bytes, &RawBuffer] { // NOLINT(modernize-avoid-c-arrays)
      return ::recv(FD, &RawBuffer, Bytes, 0);
    },
    -1);
  if (!ReadBytes)
  {
    std::errc EC = static_cast<std::errc>(ReadBytes.getError().value());
    if (EC == std::errc::interrupted /* EINTR */)
    {
      // Not an error, continue.
      Continue = true;
      return {};
    }
    if (EC == std::errc::operation_would_block /* EWOULDBLOCK */ ||
        EC == std::errc::resource_unavailable_try_again /* EAGAIN */)
    {
      // No more data left in the stream.
      Continue = false;
      return {};
    }

    std::cerr << "Socket " << Handle.get() << " - read error." << std::endl;
    Continue = false;
    setFailed();
    throw std::system_error{std::make_error_code(EC)};
  }

  Return.append(RawBuffer, ReadBytes.get());
  Continue = true;
  if (ReadBytes.get() == 0)
  {
    std::clog << "Socket " << Handle.get() << " - disconnected." << std::endl;
    setFailed();
    Continue = false;
  }
  return Return;
}

std::size_t Socket::writeImpl(std::string_view Buffer, bool& Continue)
{
  auto SentBytes = CheckedPOSIX(
    [FD = Handle.get(), Buffer = Buffer.data(), Size = Buffer.size()] {
      return ::send(FD, Buffer, Size, 0);
    },
    -1);
  if (!SentBytes)
  {
    std::errc EC = static_cast<std::errc>(SentBytes.getError().value());
    if (EC == std::errc::interrupted /* EINTR */)
    {
      // Not an error, may continue.
      Continue = true;
      return 0;
    }

    std::cerr << "Socket " << Handle.get() << " - write error." << std::endl;
    setFailed();
    Continue = false;
    throw std::system_error{std::make_error_code(EC)};
  }

  Continue = true;
  if (SentBytes.get() == 0)
  {
    std::cout << "Socket " << Handle.get() << " disconnected." << std::endl;
    setFailed();
    Continue = false;
  }

  return SentBytes.get();
}

} // namespace monomux
