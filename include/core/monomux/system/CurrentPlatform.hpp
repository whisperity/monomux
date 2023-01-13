/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include "monomux/Config.h"
#include "monomux/system/Platform.hpp"

namespace monomux::system
{

static constexpr PlatformTag CurrentPlatform =

#if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_ID_Unsupported
  PlatformTag::Unsupported
#elif MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_ID_Unix
  PlatformTag::Unix
#endif /* MONOMUX_PLATFORM_ID */

  ;

} // namespace monomux::system

#ifdef MONOMUX_PLATFORM_UNIX
#include "monomux/system/UnixPlatform.hpp"
#endif
