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
#include <cstdio>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"

#include "monomux/system/Socket.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Socket")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux
{

Socket::Socket(fd Handle, std::string Identifier, bool NeedsCleanup)
  : BufferedChannel(
      std::move(Handle), std::move(Identifier), NeedsCleanup, BUFSIZ, BUFSIZ)
{}

std::size_t Socket::optimalReadSize() const noexcept { return BUFSIZ; }
std::size_t Socket::optimalWriteSize() const noexcept { return BUFSIZ; }

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

  LOG(debug) << "Created at '" << Path << '\'';

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

  LOG(debug) << "Connected to '" << Path << '\'';

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

  LOG(trace) << "Socketified FD " << Identifier;

  Socket S{std::move(FD), std::move(Identifier), false};
  S.Owning = false;
  return S;
}

Socket::~Socket() noexcept
{
  if (needsCleanup())
  {
    auto RemoveResult =
      CheckedPOSIX([this] { return ::unlink(identifier().c_str()); }, -1);
    if (!RemoveResult)
      LOG(error) << "Failed to remove file \"" << identifier()
                 << "\" when closing the socket.\n\t" << RemoveResult.getError()
                 << ' ' << RemoveResult.getError().message();
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
  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "Listening...");
  Listening = true;
}

std::optional<Socket> Socket::accept(std::error_code* Error, bool* Recoverable)
{
  POD<struct ::sockaddr_un> SocketAddr;
  POD<::socklen_t> SocketAddrLen;

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "Accepting client...");

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
    if (EC == std::errc::too_many_files_open /* EMFILE */ ||
        EC == std::errc::too_many_files_open_in_system /* ENFILE */)
    {
      LOG_WITH_IDENTIFIER(warn)
        << "Failed to accept client: " << MaybeClient.getError() << ' '
        << MaybeClient.getError().message();
      ConsiderRecoverable = true;
    }
    else if (EC == std::errc::resource_unavailable_try_again /* EAGAIN */ ||
             EC == std::errc::interrupted /* EINTR */ ||
             EC == std::errc::connection_aborted /* ECONNABORTED */ ||
             static_cast<int>(EC) != 0)
    {
      LOG_WITH_IDENTIFIER(error)
        << "Failed to accept client: " << MaybeClient.getError() << ' '
        << MaybeClient.getError().message();
      ConsiderRecoverable = false;
    }

    if (Recoverable)
      *Recoverable = ConsiderRecoverable;
    return std::nullopt;
  }

  // Successfully accepted a client.
  std::string ClientPath = SocketAddr->sun_path;
  LOG_WITH_IDENTIFIER(trace) << "Client \"" << ClientPath << "\" connected";
  return Socket::wrap(MaybeClient.get(), std::move(ClientPath));
}

std::string Socket::readImpl(std::size_t Bytes, bool& Continue)
{
  static constexpr std::size_t BufferSize = BUFSIZ;

  std::string Return;
  Return.reserve(Bytes);

  POD<char[BufferSize]> RawBuffer;
  if (Bytes > BufferSize)
    Bytes = BufferSize;

  auto ReadBytes = CheckedPOSIX(
    [FD = Handle.get(), Bytes, &RawBuffer] {
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

    LOG_WITH_IDENTIFIER(error) << "Read error";
    Continue = false;
    setFailed();
    throw std::system_error{std::make_error_code(EC)};
  }

  Return.append(RawBuffer, ReadBytes.get());
  Continue = true;
  if (ReadBytes.get() == 0)
  {
    LOG_WITH_IDENTIFIER(error) << "Disconnected";
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
    if (EC == std::errc::operation_would_block /* EWOULDBLOCK */ ||
        EC == std::errc::resource_unavailable_try_again /* EAGAIN */)
    {
      // This is a soft error. Writing must not continue yet, but the higher
      // level API should be allowed to buffer.
      MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                        << SentBytes.getError().message());
      Continue = false;
      return 0;
    }

    LOG_WITH_IDENTIFIER(error) << "Write error";
    setFailed();
    Continue = false;
    throw std::system_error{std::make_error_code(EC)};
  }

  Continue = true;
  if (SentBytes.get() == 0)
  {
    LOG_WITH_IDENTIFIER(error) << "Disconnected";
    setFailed();
    Continue = false;
  }

  return SentBytes.get();
}

} // namespace monomux

#undef LOG_WITH_IDENTIFIER
#undef LOG
