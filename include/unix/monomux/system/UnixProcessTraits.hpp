/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include <unistd.h>

#include "monomux/system/Platform.hpp"

namespace monomux::system
{

template <> struct ProcessTraits<PlatformTag::Unix>
{
  /// Type alias for the raw process handle type on the platform.
  using raw_handle = ::pid_t;
  using RawTy = raw_handle;

  /// A magic constant representing the invalid file descriptor.
  static constexpr RawTy Invalid = -1;
};

} // namespace monomux::system
