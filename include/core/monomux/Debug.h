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

#define MONOMUX_DETAIL_CONDITIONALLY_TRUE(X)                                   \
  do                                                                           \
  {                                                                            \
    X;                                                                         \
  } while (false)
#define MONOMUX_DETAIL_CONDITIONALLY_FALSE(X) ((void)0)


#ifndef NDEBUG
/* Convenience macro that is defined to its parameter if the program is
 * compiled in debug mode, and nothing otherwise, similarly to \p assert().
 *
 * It has been turned \e ON in this build.
 */
#define MONOMUX_DEBUG(X) MONOMUX_DETAIL_CONDITIONALLY_TRUE(X)
#else /* NDEBUG */
/* Convenience macro that is defined to its parameter if the program is
 * compiled in debug mode, and nothing otherwise, similarly to \p assert().
 *
 * It has been turned \p OFF in this build.
 */
#define MONOMUX_DEBUG(X) MONOMUX_DETAIL_CONDITIONALLY_FALSE(X)
#endif /* NDEBUG */
