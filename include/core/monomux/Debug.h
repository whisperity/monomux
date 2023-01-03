/* SPDX-License-Identifier: LGPL-3.0-only */
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
