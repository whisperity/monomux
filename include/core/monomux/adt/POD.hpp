/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace monomux
{

namespace detail
{

/// Prevents optimisation of \p memset calls by too clever compilers.
inline void
// NOLINTNEXTLINE(readability-identifier-naming)
memset_manual(void* B,
              // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
              int Ch,
              std::size_t N) noexcept
{
  if (!B || !N)
    return;

  volatile auto* P = reinterpret_cast<unsigned char*>(B);
  while (N--)
    *P++ = Ch;
}

} // namespace detail

/// This class wraps a C-style struct, a "Plain Old Data" (POD) into a C++
/// structure which ensures that the data is created zero-filled.
template <typename T> struct POD
{
  static_assert(std::is_pod_v<T>, "Only supporting PODs!");
  static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                "Only supporting PODs!");

  T& operator*() { return Data; }
  const T& operator*() const { return Data; }
  T* operator&() { return &Data; }
  const T* operator&() const { return &Data; }
  T* operator->() { return &Data; }
  const T* operator->() const { return &Data; }
  operator T&() { return Data; }
  operator const T&() const { return Data; }

  /// Creates an empty space where \p T fits.
  POD() noexcept
  {
    static_assert(sizeof(*this) == sizeof(T), "Extra padding is forbidden!");
    reset();
  }
  ~POD() noexcept { reset(); }

  POD(const POD& RHS) noexcept { std::memcpy(&Data, &RHS.Data, sizeof(T)); }

  POD(POD&& RHS) noexcept
  {
    std::memcpy(&Data, &RHS.Data, sizeof(T));
    RHS.reset();
  }

  POD& operator=(const POD& RHS) noexcept
  {
    if (this == &RHS)
      return *this;

    std::memcpy(&Data, &RHS.Data, sizeof(T));
    return *this;
  }

  POD& operator=(POD&& RHS) noexcept
  {
    if (this == &RHS)
      return *this;

    std::memcpy(&Data, &RHS.Data, sizeof(T));
    RHS.reset();
    return *this;
  }

  /// Zerofill the memory area of the contained object.
  void reset() { detail::memset_manual(&Data, 0, sizeof(T)); }

private:
  T Data;
};

} // namespace monomux
