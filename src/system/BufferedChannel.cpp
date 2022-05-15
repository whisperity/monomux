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
#include <sstream>

// #include "monomux/adt/POD.hpp"

#include "monomux/system/BufferedChannel.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/BufferedChannel")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux
{

// We have need of this here.
template class RingStorage<char>;

std::string BufferedChannel::buffer_overflow::craftErrorMessage(
  const std::string& Identifier, std::size_t Size)
{
  std::ostringstream B;
  B << "Channel '" << Identifier << "' buffer overflow maximum size of "
    << BufferedChannel::BufferSizeMax << " <= actual size " << Size;
  return B.str();
}

BufferedChannel::BufferedChannel(
  fd Handle,
  std::string Identifier,
  bool NeedsCleanup,
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  std::size_t ReadBufferSize,
  std::size_t WriteBufferSize)
  : Channel(std::move(Handle), std::move(Identifier), NeedsCleanup)
{
  if (ReadBufferSize)
    Read = new Buffer(ReadBufferSize);
  if (WriteBufferSize)
    Write = new Buffer(WriteBufferSize);
}

BufferedChannel::~BufferedChannel()
{
  delete Read;
  delete Write;
  Read = nullptr;
  Write = nullptr;
}

static void throwIfFailed(bool Failed)
{
  if (Failed)
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Channel has failed."};
}
static void throwIfNoRead(bool Read)
{
  if (!Read)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Channel does not support reading."};
}
static void throwIfNoWrite(bool Write)
{
  if (!Write)
    throw std::system_error{
      std::make_error_code(std::errc::operation_not_permitted),
      "Channel does not support writing."};
}

std::string BufferedChannel::read(std::size_t Bytes)
{
  throwIfFailed(failed());
  throwIfNoRead(Read);

  std::string Return;
  Return.reserve(Bytes);

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "read(" << Bytes << ")...");
  if (std::size_t StoredBufferSize = readInBuffer())
  {
    std::size_t BytesFromBuffer = std::min(Bytes, StoredBufferSize);
    std::vector<char> V = Read->takeFront(BytesFromBuffer);
    Return.append(V.begin(), V.end());

    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "read() "
                      << "<- " << V.size() << " bytes buffer");

    Bytes -= V.size();
  }
  if (!Bytes)
    return Return;

  const std::size_t ChunkSize = optimalReadSize();
  bool ContinueReading = true;
  while (ContinueReading && Bytes > 0)
  {
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(read) "
                      << "Request " << ChunkSize << " bytes...");
    std::string Chunk = readImpl(ChunkSize, ContinueReading);
    if (Chunk.empty())
    {
      MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "(read) "
                                                   << "No more data!");
      break;
    }

    const std::size_t ReadSize = Chunk.size();
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(read) "
                      << "Received " << ReadSize << " bytes");
    if (ReadSize < ChunkSize)
      // Managed to read less data than wanted to for the current chunk.
      // Assume no more data remaining.
      ContinueReading = false;

    // Serve at most the remaining byte count into the return value.
    const std::size_t BytesFromRead = std::min(Bytes, ReadSize);
    Return.append(Chunk.begin(), Chunk.begin() + BytesFromRead);

    if (ReadSize > Bytes)
    {
      // Buffer anything that remained in the read chunk -- and thus already
      // consumed from the system resource!
      const std::size_t BytesToSave = ReadSize - Bytes;
      MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                        << "(read) "
                        << "Buffering " << BytesToSave << " bytes");
      Read->putBack(Chunk.data() + BytesFromRead, BytesToSave);
      ContinueReading = false;
    }

    Bytes -= BytesFromRead;
  }

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "read() "
                                               << "-> " << Return.size());
  return Return;
}

std::size_t BufferedChannel::write(std::string_view Data)
{
  throwIfFailed(failed());
  throwIfNoWrite(Write);

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << "write(" << Data.size() << ")...");
  const std::size_t ChunkSize = optimalWriteSize();
  bool ContinueWriting = true;

  // First, try to see if there is data in the write buffer that could be served
  // first.
  if (const std::size_t InWriteBuffer = writeInBuffer(),
      BufferSent = flushWrites();
      BufferSent < InWriteBuffer)
    // There was data in the buffer and not all of it managed to send. We can't
    // send Data because that would be an out-of-order send.
    ContinueWriting = false;

  if (!ContinueWriting)
  {
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(write) "
                      << "Buffering " << Data.size() << " bytes");
    Write->putBack(Data.data(), Data.size());
    return 0;
  }

  // If we are this point, the buffer should be clear and Data is still unsent.
  std::size_t BytesSent = 0;
  ContinueWriting = true;
  while (ContinueWriting && !Data.empty())
  {
    const std::size_t ToSend = std::min(ChunkSize, Data.size());
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "Send " << ToSend << " bytes...");

    std::string_view Chunk = Data.substr(0, std::min(ChunkSize, Data.size()));
    const std::size_t ChunkWrittenSize = writeImpl(Chunk, ContinueWriting);
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "Sent " << ChunkWrittenSize << " bytes");

    if (ChunkWrittenSize < ToSend)
      // Managed to write less data than wanted to for the current chunk.
      // This is very likely an error, and we should stop trying for now.
      // Assume no more data remaining.
      ContinueWriting = false;

    BytesSent += ChunkWrittenSize;
    Data.remove_prefix(ChunkWrittenSize);
  }

  if (!Data.empty())
  {
    // Buffer anything that remained in the write chunk -- and thus already
    // consumed from the client!
    const std::size_t BytesToSave = Data.size();
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "Buffering " << BytesToSave << " bytes");
    Write->putBack(Data.data(), BytesToSave);
  }

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "write() "
                                               << "-> " << BytesSent);
  return BytesSent;
}

std::size_t BufferedChannel::load(std::size_t Bytes)
{
  throwIfFailed(failed());
  throwIfNoRead(Read);

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "load(" << Bytes << ")...");
  const std::size_t ChunkSize = optimalReadSize();
  bool ContinueReading = true;
  std::size_t ReadBytes = 0;
  while (ContinueReading && Bytes > 0)
  {
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(load) "
                      << "Request " << ChunkSize << " bytes...");
    std::string Chunk = readImpl(ChunkSize, ContinueReading);
    if (Chunk.empty())
    {
      MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "(load) "
                                                   << "No more data!");
      break;
    }

    const std::size_t ReadSize = Chunk.size();
    ReadBytes += ReadSize;
    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(load) "
                      << "Received " << ReadSize << " bytes");
    if (ReadSize < ChunkSize)
      // Managed to read less data than wanted to for the current chunk.
      // Assume no more data remaining.
      ContinueReading = false;

    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(load) "
                      << "Storing " << ReadSize << " bytes");
    Read->putBack(Chunk.data(), ReadSize);

    Bytes -= std::min(ReadSize, Bytes);
  }

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "load() "
                                               << "-> " << ReadBytes);
  return ReadBytes;
}

std::size_t BufferedChannel::flushWrites()
{
  throwIfFailed(failed());
  throwIfNoWrite(Write);
  if (!hasBufferedWrite())
    return 0;

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << "flush(" << writeInBuffer() << ")...");
  const std::size_t ChunkSize = optimalWriteSize();
  std::size_t BytesSent = 0;
  bool ContinueWriting = true;
  while (ContinueWriting && hasBufferedWrite())
  {
    std::vector<char> V = Write->peekFront(ChunkSize);
    const std::size_t ChunkBytesSent =
      writeImpl(std::string_view{V.data(), V.size()}, ContinueWriting);
    BytesSent += ChunkBytesSent;

    MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                      << "(flush) "
                      << "<- " << ChunkBytesSent << " bytes buffer");

    if (ChunkBytesSent < V.size())
      // If we managed to send less data then the chunk size, something is
      // wrong and writing should stop. But only the actually sent bytes
      // should be removed from the buffer!
      ContinueWriting = false;

    Write->dropFront(ChunkBytesSent);
  }
  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace) << "flush() "
                                               << "-> " << BytesSent);

  return BytesSent;
}

} // namespace monomux

#undef LOG_WITH_IDENTIFIER
#undef LOG
