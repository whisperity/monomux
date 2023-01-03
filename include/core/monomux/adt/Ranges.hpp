/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <iterator>

namespace monomux::ranges
{

/// Creates a \p string_view from a range of \p string elements.
///
/// The equivalent constructor is only standardised starting C++20.
[[nodiscard]] inline std::string_view stringRange(std::string::iterator B,
                                                  std::string::iterator E)
{
  return {&*B, static_cast<std::string_view::size_type>(std::distance(B, E))};
}

template <typename T> struct ReverseView
{
  T& Collection;
};

template <typename T> auto begin(ReverseView<T> I)
{
  using std::rbegin;
  return rbegin(I.Collection);
}

template <typename T> auto end(ReverseView<T> I)
{
  using std::rend;
  return rend(I.Collection);
}

// NOLINTNEXTLINE(readability-identifier-naming)
template <typename T>[[nodiscard]] ReverseView<T> reverse_view(T&& Collection)
{
  return {Collection};
}

} // namespace monomux::ranges
