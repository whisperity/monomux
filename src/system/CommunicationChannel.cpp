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
#include "monomux/adt/POD.hpp"

#include "monomux/system/CommunicationChannel.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/CommunicationChannel")

namespace monomux
{

CommunicationChannel::CommunicationChannel(fd Handle,
                                           std::string Identifier,
                                           bool NeedsCleanup)
  : Handle(std::move(Handle)), Identifier(std::move(Identifier)),
    EntityCleanup(NeedsCleanup)
{
  ReadBuffer.reserve(BufferSize);
  WriteBuffer.reserve(BufferSize);
}

CommunicationChannel::CommunicationChannel(CommunicationChannel&& RHS) noexcept
  : Handle(std::move(RHS.Handle)), Identifier(std::move(RHS.Identifier)),
    ReadBuffer(std::move(RHS.ReadBuffer)),
    WriteBuffer(std::move(RHS.WriteBuffer)),
    EntityCleanup(std::move(RHS.EntityCleanup)), Failed(std::move(RHS.Failed))
{}

CommunicationChannel&
CommunicationChannel::operator=(CommunicationChannel&& RHS) noexcept
{
  if (this == &RHS)
    return *this;

  Handle = std::move(RHS.Handle);
  Identifier = std::move(RHS.Identifier);
  ReadBuffer = std::move(RHS.ReadBuffer);
  WriteBuffer = std::move(RHS.WriteBuffer);
  EntityCleanup = std::move(RHS.EntityCleanup);
  Failed = std::move(RHS.Failed);

  return *this;
}

fd CommunicationChannel::release() &&
{
  Identifier = "<gc:";
  Identifier.append(std::to_string(Handle.get()));
  Identifier.push_back('>');
  ReadBuffer.clear();
  WriteBuffer.clear();
  EntityCleanup = false;
  setFailed();

  return std::move(Handle);
}

std::string CommunicationChannel::read(const std::size_t Bytes)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Channel has failed."};
  MONOMUX_TRACE_LOG(LOG(trace)
                    << identifier() << ": Reading " << Bytes << " bytes");

  std::string Return;
  Return.reserve(Bytes);

  std::size_t RemainingBytes = Bytes;

  // First, try to see if there is data from previous reads stored in a buffer
  // field. If yes, we should at first serve those!
  if (!ReadBuffer.empty())
  {
    std::size_t BytesFromRB = std::min(ReadBuffer.size(), RemainingBytes);
    std::string_view V{ReadBuffer.data(), BytesFromRB};
    Return.append(V);
    RemainingBytes -= BytesFromRB;

    MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": " << BytesFromRB
                                 << " bytes served from backbuffer");

    // Consume the data from the read-buffer's front.
    ReadBuffer.erase(ReadBuffer.begin(), ReadBuffer.begin() + BytesFromRB);
  }
  if (!RemainingBytes)
    // Everything served from local buffer.
    return Return;

  // As long as there is place for more data, try reading, in chunks, from the
  // underlying resource.
  bool ContinueReading = true;
  while (ContinueReading && RemainingBytes > 0 && RemainingBytes <= Bytes)
  {
    MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Requesting "
                                 << BufferSize << " bytes...");
    std::string ReadChunk = readImpl(BufferSize, ContinueReading);
    std::size_t ReadBytes = ReadChunk.size();
    if (!ReadBytes)
    {
      MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": No further data read");
      /* ContinueReading = false; */ // Not needed because the loop breaks.
      break;
    }
    MONOMUX_TRACE_LOG(LOG(trace)
                      << identifier() << ": Read " << ReadBytes << " bytes");
    if (ReadBytes < BufferSize)
      // Managed to read less data than wanted to in the current chunk. Assume
      // no more data remaining.
      ContinueReading = false;

    std::size_t BytesFillableFromCurrentRead =
      std::min(ReadBytes, RemainingBytes);
    // Serve at most this many bytes from the current read into the return
    // value.
    Return.append(ReadChunk.begin(),
                  ReadChunk.begin() + BytesFillableFromCurrentRead);

    if (ReadBytes > RemainingBytes)
    {
      MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Storing "
                                   << ReadBytes - RemainingBytes
                                   << " read bytes in backbuffer");
      // Store anything that remained in the read chunk (and thus already
      // consumed from the system resource!) in our saving buffer.
      ReadBuffer.insert(ReadBuffer.end(),
                        ReadChunk.begin() + BytesFillableFromCurrentRead,
                        ReadChunk.end());
      ContinueReading = false;
    }

    RemainingBytes -= BytesFillableFromCurrentRead;
  }

  MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Successfully read "
                               << Return.size() << " bytes...");
  return Return;
}

std::size_t CommunicationChannel::write(std::string_view Buffer)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Channel has failed."};
  MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Writing " << Buffer.size()
                               << " bytes");

  std::size_t BytesSent = 0;

  // First, try to see if there is data in the write buffer that should be
  // served first.
  bool ContinueWriting = true;
  while (ContinueWriting && !WriteBuffer.empty())
  {
    std::string_view V{WriteBuffer.data(),
                       std::min(BufferSize, WriteBuffer.size())};
    std::size_t BytesWritten = writeImpl(V, ContinueWriting);
    BytesSent += BytesWritten;

    MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": " << BytesWritten
                                 << " bytes sent from backbuffer");

    // Discard the data thas has been written in the current step.
    WriteBuffer.erase(WriteBuffer.begin(), WriteBuffer.begin() + BytesWritten);
  }
  if (!ContinueWriting)
    return BytesSent;

  // Now that the stored buffer is empty, start writing the user's request.
  ContinueWriting = true;
  while (ContinueWriting && !Buffer.empty())
  {
    std::string_view Chunk =
      Buffer.substr(0, std::min(BufferSize, Buffer.size()));
    MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Sending " << Chunk.size()
                                 << " bytes...");
    std::size_t BytesWritten = writeImpl(Chunk, ContinueWriting);
    MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Written " << BytesWritten
                                 << " bytes");
    BytesSent += BytesWritten;

    // Discard the data that has been written in the current step.
    Buffer.remove_prefix(BytesWritten);
  }

  if (!Buffer.empty())
  {
    // If the buffer is not yet empty, but the writing backend failed, it means
    // that the send data is not sent fully, and would be lost. Save what
    // remains in our stored buffer.
    MONOMUX_TRACE_LOG(LOG(trace)
                      << identifier() << ": Keeping " << Buffer.size()
                      << " yet unwritten bytes in backbuffer");
    WriteBuffer.insert(WriteBuffer.end(), Buffer.begin(), Buffer.end());
  }

  MONOMUX_TRACE_LOG(LOG(trace) << identifier() << ": Successfully wrote "
                               << BytesSent << " bytes");

  return BytesSent;
}

} // namespace monomux

#undef LOG
