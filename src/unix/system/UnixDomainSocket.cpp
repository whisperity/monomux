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
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"

#include "monomux/system/UnixDomainSocket.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/DomainSocket")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux::system::unix
{

DomainSocket::DomainSocket(Handle FD,
                           std::string Identifier,
                           bool NeedsCleanup,
                           bool Owning)
  : Socket(std::move(FD), std::move(Identifier), NeedsCleanup, Owning)
{}

DomainSocket DomainSocket::create(std::string Path, bool InheritInChild)
{
  fd::flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd Handle = CheckedErrnoThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "socket()",
    -1);

  // TODO: Mask?

  POD<struct ::sockaddr_un> SocketAddr;
  SocketAddr->sun_family = AF_UNIX;
  std::strncpy(
    SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

  CheckedErrnoThrow(
    [&Handle, &SocketAddr] {
      return ::bind(Handle,
                    reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                    sizeof(SocketAddr));
    },
    "bind('" + Path + "')",
    -1);

  LOG(debug) << "Created at '" << Path << '\'';

  DomainSocket S{std::move(Handle), std::move(Path), true, true};
  return S;
}

DomainSocket DomainSocket::connect(std::string Path, bool InheritInChild)
{
  fd::flag_t ExtraFlags = InheritInChild ? 0 : SOCK_CLOEXEC;
  fd::raw_fd FD = CheckedErrnoThrow(
    [ExtraFlags] { return ::socket(AF_UNIX, SOCK_STREAM | ExtraFlags, 0); },
    "socket()",
    -1);

  POD<struct ::sockaddr_un> SocketAddr;
  SocketAddr->sun_family = AF_UNIX;
  std::strncpy(
    SocketAddr->sun_path, Path.c_str(), sizeof(SocketAddr->sun_path) - 1);

  CheckedErrnoThrow(
    [&FD, &SocketAddr] {
      return ::connect(FD,
                       reinterpret_cast<struct ::sockaddr*>(&SocketAddr),
                       sizeof(SocketAddr));
    },
    "connect('" + Path + "')",
    -1);

  LOG(debug) << "Connected to '" << Path << '\'';

  DomainSocket S{Handle::wrap(FD), std::move(Path), false, false};
  return S;
}

DomainSocket DomainSocket::wrap(fd&& FD, std::string Identifier)
{
  if (Identifier.empty())
  {
    Identifier.append("<sock-fd:");
    Identifier.append(std::to_string(FD.get()));
    Identifier.push_back('>');
  }

  LOG(trace) << "Socketified FD " << Identifier;

  DomainSocket S{std::move(FD), std::move(Identifier), false, false};
  return S;
}

DomainSocket::~DomainSocket() noexcept
{
  if (needsCleanup())
  {
    auto RemoveResult =
      CheckedErrno([this] { return ::unlink(identifier().c_str()); }, -1);
    if (!RemoveResult)
      LOG(error) << "Failed to remove file \"" << identifier()
                 << "\" when closing the socket.\n\t" << RemoveResult.getError()
                 << ' ' << RemoveResult.getError().message();
  }
}

std::unique_ptr<system::Socket> DomainSocket::acceptImpl(std::error_code* Error,
                                                         bool* Recoverable)
{
  POD<struct ::sockaddr_un> SocketAddr;
  POD<::socklen_t> SocketAddrLen;

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "Accepting client...");

  auto MaybeClient = CheckedErrno(
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
    return {};
  }

  // Successfully accepted a client.
  std::string ClientPath = SocketAddr->sun_path;
  LOG_WITH_IDENTIFIER(trace) << "Client \"" << ClientPath << "\" connected";
  std::unique_ptr<DomainSocket> S = std::make_unique<DomainSocket>(
    DomainSocket::wrap(MaybeClient.get(), std::move(ClientPath)));
  return S;
}

} // namespace monomux::system::unix

#undef LOG_WITH_IDENTIFIER
#undef LOG
