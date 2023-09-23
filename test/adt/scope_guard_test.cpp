/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>

#include "monomux/adt/scope_guard.hpp"

/* NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */

using namespace monomux;

TEST(ScopeGuard, EntryAndExitCalled)
{
  int Variable = 2;
  {
    ASSERT_EQ(Variable, 2);

    scope_guard SG{[&Variable] { Variable = 4; },
                   [&Variable] { Variable = 0; }};
    ASSERT_EQ(Variable, 4);
  }
  ASSERT_EQ(Variable, 0);
}

TEST(ScopeGuard, RestoreGuard)
{
  int Variable = 2;
  {
    ASSERT_EQ(Variable, 2);
    restore_guard RG{Variable};
    Variable = 4;
    ASSERT_EQ(Variable, 4);
  }
  ASSERT_EQ(Variable, 2);
}

/* NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */
