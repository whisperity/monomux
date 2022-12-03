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
#include <cstdint>
#include <string>

#include "monomux/system/CurrentPlatform.hpp"
#include "monomux/system/HandleTraits.hpp"

namespace monomux::system
{

using PlatformSpecificHandleTraits = HandleTraits<CurrentPlatform>;

/// Represents the abstract notion of a resource handle. The release of the
/// underlying OS-level resource should be taken care of at the end of the
/// life of \p Handle.
class Handle
{
public:
  using Raw = PlatformSpecificHandleTraits::RawTy;

protected:
  Raw Value;

  Handle(Raw Value) noexcept;

public:
  /// \returns the number of handles that the current process may have open.
  static std::size_t maxHandles();

  /// Creates an empty file descriptor that does not wrap anything.
  Handle() noexcept : Value(PlatformSpecificHandleTraits::Invalid) {}

  /// Wrap the raw platform resource handle into the RAII object.
  static Handle wrap(Raw Value) noexcept;

  Handle(Handle&& RHS) noexcept : Value(RHS.release()) {}
  Handle& operator=(Handle&& RHS) noexcept
  {
    if (this == &RHS)
      return *this;
    Value = RHS.release();
    return *this;
  }

  /// When the wrapper dies, if the object owned a file descriptor, close it.
  ~Handle() noexcept
  {
    if (!has())
      return;
    PlatformSpecificHandleTraits::close(release());
  }

  /// \returns true if the handle is owning a resource.
  bool has() const noexcept { return isValid(get()); }

  /// \returns true if the handle is owning a resource.
  static bool isValid(Raw Value) noexcept
  {
    return Value != PlatformSpecificHandleTraits::Invalid;
  }

  /// Convert to the system primitive type.
  operator Raw() const noexcept { return Value; }

  /// Convert to the system primitive type.
  Raw get() const noexcept { return Value; }

  /// Takes the file descriptor from the current object and changes it to not
  /// manage anything.
  [[nodiscard]] Raw release() noexcept
  {
    Raw H = Value;
    Value = PlatformSpecificHandleTraits::Invalid;
    return H;
  }

  std::string to_string() const // NOLINT(readability-identifier-naming)
  {
    return PlatformSpecificHandleTraits::to_string(Value);
  }
};

} // namespace monomux::system
