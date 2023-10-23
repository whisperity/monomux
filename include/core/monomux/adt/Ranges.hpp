/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <algorithm>
#include <iterator>

/* NOLINTBEGIN(readability-identifier-naming) */

namespace monomux::ranges
{

#define MONOMUX_RANGE_OVERLOAD_1(                                              \
  FUNCTION, TEMPLATE_PARAM, FUNCTION_PARAM_NAME, NODISCARD)                    \
  template <typename Range, typename TEMPLATE_PARAM>                           \
  NODISCARD auto FUNCTION(Range&& R, TEMPLATE_PARAM&& FUNCTION_PARAM_NAME)     \
  {                                                                            \
    using std::begin, std::end;                                                \
    return std::FUNCTION(                                                      \
      begin(R), end(R), std::forward<TEMPLATE_PARAM>(FUNCTION_PARAM_NAME));    \
  }

MONOMUX_RANGE_OVERLOAD_1(all_of, Predicate, P, [[nodiscard]]);
MONOMUX_RANGE_OVERLOAD_1(any_of, Predicate, P, [[nodiscard]]);
MONOMUX_RANGE_OVERLOAD_1(find_if, Predicate, P, [[nodiscard]]);
MONOMUX_RANGE_OVERLOAD_1(none_of, Predicate, P, [[nodiscard]]);

MONOMUX_RANGE_OVERLOAD_1(find, T, V, [[nodiscard]]);

#undef MONOMUX_RANGE_OVERLOAD_1

template <typename Range, typename T>
[[nodiscard]] bool contains(Range&& R, T&& V)
{
  using std::begin, std::end;
  return std::find(begin(R), end(R), std::forward<T>(V)) != end(R);
}

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

template <typename T> [[nodiscard]] auto begin(ReverseView<T> I)
{
  using std::rbegin;
  return rbegin(I.Collection);
}

template <typename T> [[nodiscard]] auto end(ReverseView<T> I)
{
  using std::rend;
  return rend(I.Collection);
}

template <typename T> [[nodiscard]] ReverseView<T> reverse_view(T&& Collection)
{
  return {Collection};
}

} // namespace monomux::ranges

/* NOLINTEND(readability-identifier-naming) */
