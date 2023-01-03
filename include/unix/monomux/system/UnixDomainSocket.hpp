/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include "monomux/system/UnixSocket.hpp"

namespace monomux::system::unix
{

/// This class wraps a Unix domain socket (appearing to applications as a named
/// file in the filesystem) and allows reading or writing to the socket.
///
/// This implementation uses \p SOCK_STREAM.
class DomainSocket : public Socket
{
public:
  /// Creates a new \p DomainSocket which will be owned by the current
  /// instance, and removed on exit. Such sockets can be used to await
  /// connections and implement server-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see bind(2)
  [[nodiscard]] static DomainSocket create(std::string Path,
                                           bool InheritInChild = false);

  /// Opens a connection to the socket existing in the file system at \p Path.
  /// The connection will be cleaned up during destruction, but the file entity
  /// is left intact. A low-level connection is initiated through the socket.
  /// Such sockets can be used to implement clients-like behaviour.
  ///
  /// If \p InheritInChild is true, the socket will be flagged to be inherited
  /// by a potential child process.
  ///
  /// \see connect(2)
  [[nodiscard]] static DomainSocket connect(std::string Path,
                                            bool InheritInChild = false);

  /// Wraps an already existing file descriptor, \p FD as a socket.
  /// Ownership of the resource itself is taken by the \p Socket instance and
  /// the file will be closed during destruction, but no additional cleanup
  /// may take place.
  ///
  /// \param Identifier An identifier to assign to the \p Socket. If empty, a
  /// default value will be created.
  ///
  /// \note This method does \b NOT verify whether the wrapped file descriptor
  /// is indeed a socket, and assumes it is set up (either in server, or client
  /// mode) already.
  [[nodiscard]] static DomainSocket wrap(fd&& FD, std::string Identifier);

  ~DomainSocket() noexcept override;
  DomainSocket(DomainSocket&&) noexcept = default;
  DomainSocket& operator=(DomainSocket&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

protected:
  DomainSocket(Handle FD,
               std::string Identifier,
               bool NeedsCleanup,
               bool Owning);

  /// \see accept(2)
  [[nodiscard]] std::unique_ptr<system::Socket>
  acceptImpl(std::error_code* Error, bool* Recoverable) override;
};

} // namespace monomux::system::unix
