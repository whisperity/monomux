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

namespace monomux::_detail
{

/// If executed during runtime, kills the program and prints the specified
/// message to the standard error stream.
[[noreturn]] void
// NOLINTNEXTLINE(readability-identifier-naming)
unreachable_impl(const char* Msg = nullptr,
                 const char* File = nullptr,
                 std::size_t LineNo = 0);

} // namespace monomux::_detail

#ifndef NDEBUG
#define unreachable(MSG)                                                       \
  ::monomux::_detail::unreachable_impl(MSG, __FILE__, __LINE__)
#else
#define unreachable(MSG) ::monomux::_detail::unreachable_impl(MSG, nullptr, 0)
#endif
