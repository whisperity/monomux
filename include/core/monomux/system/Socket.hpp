/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <string>
#include <system_error>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/BufferedChannel.hpp"

namespace monomux::system
{

/// A socket is a two-way communication channel between a "client" and a
/// "server".
///
/// Sockets are stream-based, which is similar to TCP connections: clients
/// connect to the server socket and then a connection is accepted, at which
/// point the connection \e towards that new client becomes its own \p Socket.
///
/// \note This object does \b NOT manage the set of connected clients or
/// anything related to such notion, only exposes the low-level system calls
/// facilitating socket behaviour.
class Socket : public BufferedChannel
{
public:
  /// Starts listening for incoming connection on the current socket.
  /// This is only valid if the current socket was created in full ownership
  /// mode.
  void listen(std::size_t QueueSize);

  /// Accepts a new connection on the current serving socket. This operation
  /// \b MAY block. This call is only valid if the current socket was created in
  /// full ownership mode, and \p listen() had already been called for it.
  ///
  /// \param Error If non-null and the accepting of the client fails, the error
  /// code is returned in this parameter.
  /// \param Recoverable If non-null and the accepting of the client fails, but
  /// the low-level error is indicative of a potential to try again, will be set
  /// to \p true.
  std::unique_ptr<Socket> accept(std::error_code* Error = nullptr,
                                 bool* Recoverable = nullptr);

  ~Socket() noexcept override = default;
  Socket(Socket&&) noexcept = default;
  Socket& operator=(Socket&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

  [[nodiscard]] std::size_t optimalReadSize() const noexcept override;
  [[nodiscard]] std::size_t optimalWriteSize() const noexcept override;

protected:
  Socket(Handle FD,
         std::string Identifier,
         bool NeedsCleanup,
         std::size_t BufferSize,
         bool Owning);

  virtual void listenImpl(std::size_t QueueSize) = 0;
  [[nodiscard]] virtual std::unique_ptr<Socket>
  acceptImpl(std::error_code* Error, bool* Recoverable) = 0;

  /// Whether the current instance is \e owning a socket, i.e. controlling it
  /// as a server.
  UniqueScalar<bool, false> Owning;
  /// Whether the current instance is \e listening for incoming connections
  /// via \p listen().
  UniqueScalar<bool, false> Listening;
};

} // namespace monomux::system
