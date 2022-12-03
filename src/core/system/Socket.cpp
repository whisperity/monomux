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
#include <utility>

#include "monomux/system/Socket.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Socket")

namespace monomux::system
{

static constexpr std::size_t DefaultSocketBuffer = 512;

Socket::Socket(Handle FD,
               std::string Identifier,
               bool NeedsCleanup,
               std::size_t BufferSize,
               bool Owning)
  : BufferedChannel(std::move(FD),
                    std::move(Identifier),
                    NeedsCleanup,
                    BufferSize,
                    BufferSize),
    Owning(Owning)
{}

std::size_t Socket::optimalReadSize() const noexcept
{
  return DefaultSocketBuffer;
}
std::size_t Socket::optimalWriteSize() const noexcept
{
  return DefaultSocketBuffer;
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

  listenImpl(QueueSize);
}

std::unique_ptr<Socket> Socket::accept(std::error_code* Error,
                                       bool* Recoverable)
{
  if (!Listening)
    throw std::system_error{
      std::make_error_code(std::errc::operation_in_progress),
      "The socket is not listening!"};

  return acceptImpl(Error, Recoverable);
}

} // namespace monomux::system

#undef LOG
