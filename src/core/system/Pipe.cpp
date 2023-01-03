/* SPDX-License-Identifier: LGPL-3.0-only */
#include <utility>

#include "monomux/system/Pipe.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Pipe")

namespace monomux::system
{

static constexpr std::size_t DefaultPipeBuffer = 512;

Pipe::Pipe(Handle FD,
           std::string Identifier,
           bool NeedsCleanup,
           Mode OpenMode,
           std::size_t BufferSize)
  : BufferedChannel(std::move(FD),
                    std::move(Identifier),
                    NeedsCleanup,
                    OpenMode == Read ? BufferSize : 0,
                    OpenMode == Write ? BufferSize : 0),
    OpenedAs(OpenMode)
{}

std::size_t Pipe::optimalReadSize() const noexcept { return DefaultPipeBuffer; }
std::size_t Pipe::optimalWriteSize() const noexcept
{
  return DefaultPipeBuffer;
}

Pipe::~Pipe() noexcept
{
  if (FD && Weak)
    // Steal the file descriptor from the management object and do not let
    // the destruction actually close the resource - we do NOT own those handles
    // internally!
    (void)std::move(*this).release().release();
}

std::unique_ptr<Pipe> Pipe::AnonymousPipe::takeRead()
{
  if (!Read)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Read end of pipe already taken."};
  if (Write)
    Write.reset();
  return std::move(Read);
}

std::unique_ptr<Pipe> Pipe::AnonymousPipe::takeWrite()
{
  if (!Write)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Write end of pipe already taken."};
  if (Read)
    Read.reset();
  return std::move(Write);
}

} // namespace monomux::system

#undef LOG
