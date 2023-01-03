/* SPDX-License-Identifier: LGPL-3.0-only */
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
  [[nodiscard]] static std::size_t maxHandles();

  /// Creates an empty file descriptor that does not wrap anything.
  Handle() noexcept : Value(PlatformSpecificHandleTraits::Invalid) {}

  /// Wrap the raw platform resource handle into the RAII object.
  [[nodiscard]] static Handle wrap(Raw Value) noexcept;

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
  [[nodiscard]] bool has() const noexcept { return isValid(get()); }

  /// \returns true if the handle is owning a resource.
  [[nodiscard]] static bool isValid(Raw Value) noexcept
  {
    return Value != PlatformSpecificHandleTraits::Invalid;
  }

  /// Convert to the system primitive type.
  operator Raw() const noexcept { return Value; }

  /// Convert to the system primitive type.
  [[nodiscard]] Raw get() const noexcept { return Value; }

  /// Takes the file descriptor from the current object and changes it to not
  /// manage anything.
  [[nodiscard]] Raw release() noexcept
  {
    Raw H = Value;
    Value = PlatformSpecificHandleTraits::Invalid;
    return H;
  }

  [[nodiscard]] std::string
  to_string() const // NOLINT(readability-identifier-naming)
  {
    return PlatformSpecificHandleTraits::to_string(Value);
  }
};

} // namespace monomux::system
