/* SPDX-License-Identifier: LGPL-3.0-only */
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
