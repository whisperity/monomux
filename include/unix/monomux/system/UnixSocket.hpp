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
#pragma once
#include <optional>
#include <string>
#include <system_error>

#include "monomux/system/Socket.hpp"
#include "monomux/system/fd.hpp"

namespace monomux::system::unix
{

/// This class wraps a POSIX sockets and allows reading or writing to the
/// socket.
///
/// \see socket(7)
class Socket : public system::Socket
{
public:
  ~Socket() noexcept override = default;
  Socket(Socket&&) noexcept = default;
  Socket& operator=(Socket&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

  std::size_t optimalReadSize() const noexcept override;
  std::size_t optimalWriteSize() const noexcept override;

protected:
  Socket(Handle FD, std::string Identifier, bool NeedsCleanup, bool Owning);

  /// \see listen(2)
  void listenImpl(std::size_t QueueSize) override;

  std::unique_ptr<system::Socket> acceptImpl(std::error_code* Error,
                                             bool* Recoverable) override;

  std::string readImpl(std::size_t Bytes, bool& Continue) override;
  std::size_t writeImpl(std::string_view Buffer, bool& Continue) override;
};

} // namespace monomux::system::unix
