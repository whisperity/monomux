/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cstdint>

namespace monomux::detail
{

/// If executed during runtime, kills the program and prints the specified
/// message to the standard error stream.
[[noreturn]] void
// NOLINTNEXTLINE(readability-identifier-naming)
unreachable_impl(const char* Msg = nullptr,
                 const char* File = nullptr,
                 std::size_t LineNo = 0);

} // namespace monomux::detail

extern "C" [[noreturn]] void
// NOLINTNEXTLINE(readability-identifier-naming)
monomux_unreachable_impl(const char* Msg = nullptr,
                         const char* File = nullptr,
                         std::size_t LineNo = 0);

#if __cplusplus >= 201103L
#define MONOMUX_UNREACHABLE_FUNCTION ::monomux::detail::unreachable_impl
#else
#define MONOMUX_UNREACHABLE_FUNCTION monomux_unreachable_impl
#endif /* __cplusplus 11 */

#ifndef NDEBUG
#define unreachable(MSG) MONOMUX_UNREACHABLE_FUNCTION(MSG, __FILE__, __LINE__)
#else
#define unreachable(MSG) MONOMUX_UNREACHABLE_FUNCTION(MSG, nullptr, 0)
#endif
