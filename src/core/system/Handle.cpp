/* SPDX-License-Identifier: LGPL-3.0-only */
#include "monomux/system/Handle.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Handle")

namespace monomux::system
{

Handle::Handle(Raw Value) noexcept : Value(Value)
{
  MONOMUX_TRACE_LOG(LOG(data) << "Handle #" << Value << " owned by instance.");
}

Handle Handle::wrap(Raw Value) noexcept
{
  MONOMUX_TRACE_LOG(LOG(data) << "Handle #" << Value << " wrapped.");
  return Handle{Value};
}

} // namespace monomux::system

#undef LOG
