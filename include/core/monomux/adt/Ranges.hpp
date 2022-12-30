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
