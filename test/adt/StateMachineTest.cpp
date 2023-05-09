/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>

#include "monomux/adt/StateMachine.hpp"

/* NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */

using namespace monomux::state_machine;

static inline constexpr auto DummyLambda = [](int&) {};

static inline const auto Transition1 =
  detail::Base<int, void>::Transition<0, 1, 2>{};
static inline const auto Transition2 =
  detail::Base<int, int>::Transition<3, 4, 5>{};
static inline const auto Transition3 =
  detail::Base<int, int>::Transition<6, 7, 8, decltype(DummyLambda)>{};
static inline const auto Transition4 =
  detail::Base<int, void>::transition<0, 1, 2>();
static inline const auto Transition5 =
  detail::Base<int, int>::transition<3, 4, 5>();
static inline const auto Transition6 =
  detail::Base<int, int>::transition<6, 7, 8>(DummyLambda);
// This should NOT compile:
#if 0
static inline const auto Transition7 =
  detail::Base<int, void>::Transition<6, 7, 8>{[](int&) {}};
static inline const auto Transition8 =
  detail::Base<int, void>::transition<6, 7, 8>([](int&) {});
#endif

TEST(MetaprogrammingStateMachine, Trivial)
{
  auto Machine = createMachine<char>();
  EXPECT_EQ(Machine.CurrentStateIndex, 1);

  auto M2 = Machine.switchToState<1>();
  EXPECT_EQ(Machine.CurrentStateIndex, 1);

  auto M3 = M2.addOrTraverseTransition<'A'>();
  EXPECT_EQ(M3.CurrentStateIndex, 2);

  auto M4 = M3.addOrTraverseTransition<'B'>();
  EXPECT_EQ(M4.CurrentStateIndex, 3);

  auto M5 = M4.switchToState<1>();
  EXPECT_EQ(M5.CurrentStateIndex, 1);

  auto M6 = M5.addOrTraverseTransition<'A'>();
  EXPECT_EQ(M6.CurrentStateIndex, 2);

  auto M7 = M6.switchToState<1>();
  EXPECT_EQ(M7.CurrentStateIndex, 1);

  auto M8 = M7.addOrTraverseTransition<'C'>();
  EXPECT_EQ(M8.CurrentStateIndex, 4);

  auto RuntimeMachine = compile(M8);
  EXPECT_EQ(RuntimeMachine.getCurrentState(), Machine.CurrentStateIndex);

  RuntimeMachine('A');
  EXPECT_EQ(RuntimeMachine.getCurrentState(), M3.CurrentStateIndex);

  RuntimeMachine('B');
  EXPECT_EQ(RuntimeMachine.getCurrentState(), M4.CurrentStateIndex);

  RuntimeMachine('C');
  EXPECT_TRUE(RuntimeMachine.hasErrored());

  RuntimeMachine.reset();
  EXPECT_FALSE(RuntimeMachine.hasErrored());
  EXPECT_EQ(RuntimeMachine.getCurrentState(), Machine.CurrentStateIndex);

  RuntimeMachine('C');
  EXPECT_FALSE(RuntimeMachine.hasErrored());
  EXPECT_EQ(RuntimeMachine.getCurrentState(), M8.CurrentStateIndex);

  RuntimeMachine('Z'); // Unmapped value.
  EXPECT_TRUE(RuntimeMachine.hasErrored());

  RuntimeMachine.reset();
  EXPECT_FALSE(RuntimeMachine.hasErrored());

  RuntimeMachine('a'); // Unmapped value.
  EXPECT_TRUE(RuntimeMachine.hasErrored());
}

/* NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */
