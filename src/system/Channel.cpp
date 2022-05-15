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
#include "monomux/system/Channel.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Channel")
#define LOG_WITH_IDENTIFIER(SEVERITY) LOG(SEVERITY) << identifier() << ": "

namespace monomux
{

Channel::Channel(fd Handle, std::string Identifier, bool NeedsCleanup)
  : Handle(std::move(Handle)), Identifier(std::move(Identifier)),
    EntityCleanup(NeedsCleanup)
{}

fd Channel::release() &&
{
  Identifier = "<gc:";
  Identifier.append(std::to_string(Handle.get()));
  Identifier.push_back('>');
  EntityCleanup = false;

  return std::move(Handle);
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

} // namespace monomux

#undef LOG_WITH_IDENTIFIER
#undef LOG
