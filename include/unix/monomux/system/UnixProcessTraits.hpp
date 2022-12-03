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
#pragma once
#include <string>

#include <unistd.h>

#include "monomux/system/Platform.hpp"

namespace monomux::system
{

template <> struct ProcessTraits<PlatformTag::UNIX>
{
  /// Type alias for the raw process handle type on the platform.
  using raw_handle = ::pid_t;
  using RawTy = raw_handle;

  /// A magic constant representing the invalid file descriptor.
  static constexpr RawTy Invalid = -1;
};

} // namespace monomux::system