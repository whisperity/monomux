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
