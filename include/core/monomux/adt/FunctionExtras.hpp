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
#include <type_traits>

#define MONOMUX_DETAIL_FUNCTION_HEAD(RET_TY, NAME, ARGUMENTS, QUALIFIERS)      \
  RET_TY NAME(ARGUMENTS) QUALIFIERS

#define MONOMUX_DETAIL_FUNCTION_TEMPLATE_HEAD(                                 \
  TEMPLATES, RET_TY, NAME, ARGUMENTS, QUALIFIERS)                              \
  template <TEMPLATES>                                                         \
  MONOMUX_DETAIL_FUNCTION_HEAD(RET_TY, NAME, ARGUMENTS, QUALIFIERS)

#define MONOMUX_DETAIL_CONST_TYPE                                              \
  using Const = std::add_pointer_t<                                            \
    std::add_const_t<std::remove_pointer_t<decltype(this)>>>
#define MONOMUX_DETAIL_CONST_OBJ const_cast<Const>(this)
#define MONOMUX_DETAIL_CONST_VALUE(CALL) const auto& Value = CALL
// NOLINTBEGIN(bugprone-macro-parantheses)
#define MONOMUX_DETAIL_RETURN_CAST(RET_TY, OBJ) return const_cast<RET_TY>(OBJ)
// NOLINTEND(bugprone-macro-parantheses)

#define MONOMUX_DETAIL_FUNCTION_BODY(RET_TY, CALL)                             \
  {                                                                            \
    MONOMUX_DETAIL_CONST_TYPE;                                                 \
    MONOMUX_DETAIL_CONST_VALUE(CALL);                                          \
    MONOMUX_DETAIL_RETURN_CAST(RET_TY, Value);                                 \
  }

#define MONOMUX_DETAIL_CALL_0(NAME) MONOMUX_DETAIL_CONST_OBJ->NAME()
#define MONOMUX_DETAIL_CALL_1(NAME, ARG_1) MONOMUX_DETAIL_CONST_OBJ->NAME(ARG_1)
#define MONOMUX_DETAIL_CALL_0_T1(NAME, TYPE_1)                                 \
  MONOMUX_DETAIL_CONST_OBJ->NAME<TYPE_1>()
#define MONOMUX_DETAIL_CALL_1_T1(NAME, TYPE_1, ARG_1)                          \
  MONOMUX_DETAIL_CONST_OBJ->NAME<TYPE_1>(ARG_1)


#define MONOMUX_MEMBER_0(RETURN_TYPE, NAME, NOEXCEPT)                          \
  MONOMUX_DETAIL_FUNCTION_HEAD(RETURN_TYPE, NAME, , NOEXCEPT)                  \
  MONOMUX_DETAIL_FUNCTION_BODY(RETURN_TYPE, MONOMUX_DETAIL_CALL_0(NAME))
#define MONOMUX_MEMBER_1(RETURN_TYPE, NAME, NOEXCEPT, ARG_1_TYPE, ARG_1)       \
  MONOMUX_DETAIL_FUNCTION_HEAD(RETURN_TYPE, NAME, ARG_1_TYPE ARG_1, NOEXCEPT)  \
  MONOMUX_DETAIL_FUNCTION_BODY(RETURN_TYPE, MONOMUX_DETAIL_CALL_1(NAME, ARG_1))

#define MONOMUX_MEMBER_T1_0(RETURN_TYPE, NAME, NOEXCEPT, TYPE_1_TYPE, TYPE_1)  \
  MONOMUX_DETAIL_FUNCTION_TEMPLATE_HEAD(                                       \
    TYPE_1_TYPE TYPE_1, RETURN_TYPE, NAME, , NOEXCEPT)                         \
  MONOMUX_DETAIL_FUNCTION_BODY(RETURN_TYPE,                                    \
                               MONOMUX_DETAIL_CALL_0_T1(NAME, TYPE_1))
#define MONOMUX_MEMBER_T1_1(                                                   \
  RETURN_TYPE, NAME, NOEXCEPT, TYPE_1_TYPE, TYPE_1, ARG_1_TYPE, ARG_1)         \
  MONOMUX_DETAIL_FUNCTION_TEMPLATE_HEAD(                                       \
    TYPE_1_TYPE TYPE_1, RETURN_TYPE, NAME, ARG_1_TYPE ARG_1, NOEXCEPT)         \
  MONOMUX_DETAIL_FUNCTION_BODY(RETURN_TYPE,                                    \
                               MONOMUX_DETAIL_CALL_1_T1(NAME, TYPE_1, ARG_1))


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
