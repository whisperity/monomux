/* SPDX-License-Identifier: LGPL-3.0-only */
#include <utility>

#include "monomux/system/Channel.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Channel")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux::system
{

Channel::Channel(Handle FD, std::string Identifier, bool NeedsCleanup)
  : FD(std::move(FD)), Identifier(std::move(Identifier)),
    EntityCleanup(NeedsCleanup)
{}

Handle Channel::release() &&
{
  Identifier = "<gc:";
  Identifier.append(FD.to_string());
  Identifier.push_back('>');
  EntityCleanup = false;

  return std::move(FD);
}

std::string Channel::read(std::size_t Bytes)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Channel has failed."};

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << "Reading " << Bytes << " bytes...");
  bool Unused;
  return readImpl(Bytes, Unused);
}

std::size_t Channel::write(std::string_view Buffer)
{
  if (failed())
    throw std::system_error{std::make_error_code(std::errc::io_error),
                            "Channel has failed."};

  MONOMUX_TRACE_LOG(LOG_WITH_IDENTIFIER(trace)
                    << "Writing " << Buffer.size() << " bytes...");
  bool Unused;
  return writeImpl(Buffer, Unused);
}

} // namespace monomux::system

#undef LOG_WITH_IDENTIFIER
#undef LOG
