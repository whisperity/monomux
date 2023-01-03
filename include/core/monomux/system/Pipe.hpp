/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <string>
#include <utility>

#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/BufferedChannel.hpp"
#include "monomux/system/Handle.hpp"

namespace monomux::system
{

/// A pipe is a one-way communication channel between a reading and a writing
/// end.
///
/// Data written to the pipe's write end is buffered by the kernel and
/// can be read on the read end.
class Pipe : public BufferedChannel
{
public:
  /// The mode with which the \p Pipe is opened.
  enum Mode
  {
    /// Sentinel.
    None = 0,

    /// Open the read end of the pipe.
    Read = 1,

    /// Open the write end of the pipe.
    Write = 2
  };

  /// Wrapper for the return type of \p create() which creates an anonymous
  /// pipe that only exists as file descriptors, but not named entities.
  struct AnonymousPipe
  {
    AnonymousPipe(std::unique_ptr<Pipe>&& Read, std::unique_ptr<Pipe>&& Write)
      : Read(std::move(Read)), Write(std::move(Write))
    {}

    /// \returns the pipe for the read end.
    [[nodiscard]] Pipe* getRead() const noexcept { return Read.get(); }
    /// \returns the pipe for the write end.
    [[nodiscard]] Pipe* getWrite() const noexcept { return Write.get(); }

    /// Take ownership for the read end of the pipe, and close the write end.
    [[nodiscard]] std::unique_ptr<Pipe> takeRead();
    /// Take ownership for the write end of the pipe, and close the read end.
    [[nodiscard]] std::unique_ptr<Pipe> takeWrite();

  private:
    friend class Pipe;

    std::unique_ptr<Pipe> Read;
    std::unique_ptr<Pipe> Write;
  };

  ~Pipe() noexcept override;
  Pipe(Pipe&&) noexcept = default;
  Pipe& operator=(Pipe&&) noexcept = default;

  using BufferedChannel::read;
  using BufferedChannel::write;

  [[nodiscard]] std::size_t optimalReadSize() const noexcept override;
  [[nodiscard]] std::size_t optimalWriteSize() const noexcept override;

protected:
  Pipe(Handle FD,
       std::string Identifier,
       bool NeedsCleanup,
       Mode OpenMode,
       std::size_t BufferSize);

  UniqueScalar<Mode, None> OpenedAs;
  UniqueScalar<bool, false> Weak;
};

} // namespace monomux::system
