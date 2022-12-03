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

#define MEMBER_FN_NON_CONST_0(RETURN_TYPE, FUNCTION_NAME)                      \
  RETURN_TYPE FUNCTION_NAME()                                                  \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME());                        \
  }

#define MEMBER_FN_NON_CONST_0_NOEXCEPT(RETURN_TYPE, FUNCTION_NAME)             \
  RETURN_TYPE FUNCTION_NAME() noexcept                                         \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME());                        \
  }

#define MEMBER_FN_NON_CONST_1(                                                 \
  RETURN_TYPE, FUNCTION_NAME, ARG_1_TYPE, ARG_1_NAME)                          \
  RETURN_TYPE FUNCTION_NAME(ARG_1_TYPE ARG_1_NAME)                             \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME(ARG_1_NAME));              \
  }

#define MEMBER_FN_NON_CONST_1_NOEXCEPT(                                        \
  RETURN_TYPE, FUNCTION_NAME, ARG_1_TYPE, ARG_1_NAME)                          \
  RETURN_TYPE FUNCTION_NAME(ARG_1_TYPE ARG_1_NAME) noexcept                    \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME(ARG_1_NAME));              \
  }

namespace monomux
{

namespace detail
{

using IndexTy = unsigned long long;

template <IndexTy N, typename T0, typename... Ts> struct TypeVecAccess
{
  using type = typename TypeVecAccess<N - 1, Ts...>::type;
};

template <typename T0, typename... Ts> struct TypeVecAccess<1, T0, Ts...>
{
  using type = T0;
};

template <detail::IndexTy N, typename... Ts> struct ArgumentType;

template <detail::IndexTy N, typename RetTy, typename... Args>
struct ArgumentType<N, RetTy(Args...)>
{
  using type = typename TypeVecAccess<N, Args...>::type;
};

template <detail::IndexTy N, typename RetTy, typename... Args>
struct ArgumentType<N, RetTy(Args...) noexcept>
  : ArgumentType<N, RetTy(Args...)>
{};
template <detail::IndexTy N, typename RetTy, typename... Args>
struct ArgumentType<N, RetTy (*)(Args...)> : ArgumentType<N, RetTy(Args...)>
{};
template <detail::IndexTy N, typename RetTy, typename... Args>
struct ArgumentType<N, RetTy (&)(Args...)> : ArgumentType<N, RetTy(Args...)>
{};

template <typename> struct ReturnType;

template <typename RetTy, typename... Args> struct ReturnType<RetTy(Args...)>
{
  using type = RetTy;
};

template <typename RetTy, typename... Args>
struct ReturnType<RetTy(Args...) noexcept> : ReturnType<RetTy(Args...)>
{};
template <typename RetTy, typename... Args>
struct ReturnType<RetTy (*)(Args...)> : ReturnType<RetTy(Args...)>
{};
template <typename RetTy, typename... Args>
struct ReturnType<RetTy (&)(Args...)> : ReturnType<RetTy(Args...)>
{};

} // namespace detail

/// \returns the Nth argument type of the specified function, which must be a
/// uniquely resolved overload.
template <detail::IndexTy N, typename Fn>
using argument_t = typename detail::ArgumentType<N, Fn>::type;

/// \returns the return type of the specified function, which must be a
/// uniquely resolved overload.
template <typename Fn> using return_t = typename detail::ReturnType<Fn>::type;

} // namespace monomux
