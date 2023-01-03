/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace monomux
{

/// Tags a pointer to a type at compile-time with a scalar value that is only
/// descript to clients consuming this object.
/// Otherwise, behaves the same as \p T*.
template <std::size_t N, typename T> class Tagged
{
  static constexpr std::size_t Kind = N;
  T* Ptr;

public:
  Tagged(T* P) noexcept : Ptr(P) {}

  /// Retrieve the raw tag value.
  [[nodiscard]] std::size_t kind() const noexcept { return Kind; }
  /// Retrieve the tag value cast to the enum type \p E.
  template <typename E>[[nodiscard]] E kindAs() const noexcept
  {
    return static_cast<E>(Kind);
  }

  bool operator==(Tagged RHS) const { return Ptr == RHS.Ptr; }
  bool operator!=(Tagged RHS) const { return Ptr != RHS.Ptr; }

  T* operator->() noexcept { return Ptr; }
  const T* operator->() const noexcept { return Ptr; }
  T& operator*() noexcept
  {
    assert(Ptr);
    return *Ptr;
  }
  const T& operator*() const noexcept
  {
    assert(Ptr);
    return *Ptr;
  }
  T* get() noexcept { return Ptr; }
  const T* get() const noexcept { return Ptr; }
};

} // namespace monomux
