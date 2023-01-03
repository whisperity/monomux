/* SPDX-License-Identifier: LGPL-3.0-only */
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

  [[nodiscard]] std::unique_ptr<system::Socket>
  acceptImpl(std::error_code* Error, bool* Recoverable) override;

  [[nodiscard]] std::string readImpl(std::size_t Bytes,
                                     bool& Continue) override;
  std::size_t writeImpl(std::string_view Buffer, bool& Continue) override;
};

} // namespace monomux::system::unix
