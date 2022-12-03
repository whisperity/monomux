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
#include <system_error>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"

#include "monomux/system/UnixSocket.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Socket")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux::system::unix
{

Socket::Socket(Handle FD,
               std::string Identifier,
               bool NeedsCleanup,
               bool Owning)
  : system::Socket(
      std::move(FD), std::move(Identifier), NeedsCleanup, BUFSIZ, Owning)
{}

std::size_t Socket::optimalReadSize() const noexcept { return BUFSIZ; }
std::size_t Socket::optimalWriteSize() const noexcept { return BUFSIZ; }

void Socket::listenImpl(std::size_t QueueSize)
{
  CheckedErrnoThrow(
    [this, QueueSize] { return ::listen(raw(), static_cast<int>(QueueSize)); },
    "listen()",
    -1);
  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "Listening...");
  Listening = true;
}

std::unique_ptr<system::Socket> Socket::acceptImpl(std::error_code* /*Error*/,
                                                   bool* /*Recoverable*/)
{
  throw std::system_error{
    std::make_error_code(std::errc::operation_canceled),
    "Cannot accept() without knowing the socket implementation type!"};
}

std::string Socket::readImpl(std::size_t Bytes, bool& Continue)
{
  static constexpr std::size_t BufferSize = BUFSIZ;

  std::string Return;
  Return.reserve(Bytes);

  POD<char[BufferSize]> RawBuffer;
  if (Bytes > BufferSize)
    Bytes = BufferSize;

  auto ReadBytes = CheckedErrno(
    [FD = raw(), Bytes, &RawBuffer] {
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
  auto SentBytes = CheckedErrno(
    [FD = raw(), Buffer = Buffer.data(), Size = Buffer.size()] {
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

} // namespace monomux::system::unix

#undef LOG_WITH_IDENTIFIER
#undef LOG
