/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "monomux/Config.h"
#include "monomux/system/Platform.hpp"

namespace monomux::system
{

static constexpr PlatformTag CurrentPlatform = PlatformTag::Unix;

} // namespace monomux::system

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/UnixPlatform.hpp"
#endif
