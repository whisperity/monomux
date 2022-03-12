/**
 *
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
#include <gtest/gtest.h>

#include "adt/SmallIndexMap.hpp"

using namespace monomux;

TEST(SmallIndexMap, Integral)
{
  SmallIndexMap<int, 4> M;
  ASSERT_EQ(M.size(), 0);
  ASSERT_TRUE(M.isSmall());
  M[0] = 1;
  M[1] = 2;
  M[2] = 3;
  M[3] = 4;
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isSmall());

  EXPECT_EQ(M.get(0), 1);
  EXPECT_EQ(M.get(1), 2);
  EXPECT_EQ(M.get(2), 3);
  EXPECT_EQ(M.get(3), 4);

  M[32] = 64;
  M[64] = 128;
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  M.erase(0);
  M.erase(1);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isLarge());

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(2), nullptr);
  EXPECT_EQ(*M.tryGet(2), 3);

  M.erase(2);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());
}

TEST(SmallIndexMap, Pointer)
{
  int* I1 = new int(1);
  int* I2 = new int(2);
  int* I3 = new int(3);
  int* I4 = new int(4);

  SmallIndexMap<int*, 4> M;
  ASSERT_EQ(M.size(), 0);
  ASSERT_TRUE(M.isSmall());
  M[0] = I1;
  M[1] = I2;
  M[2] = I3;
  M[3] = I4;
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isSmall());

  EXPECT_EQ(*M.get(0), 1);
  EXPECT_EQ(*M.get(1), 2);
  EXPECT_EQ(*M.get(2), 3);
  EXPECT_EQ(*M.get(3), 4);

  int* I64 = new int(64);
  int* I128 = new int(128);

  M[32] = I64;
  M[64] = I128;
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  M.erase(0);
  M.erase(1);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isLarge());

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(2), nullptr);
  EXPECT_EQ(*M.tryGet(2), I3);
  EXPECT_EQ(**M.tryGet(2), 3);

  M.erase(2);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  delete I1;
  delete I2;
  delete I3;
  delete I4;
  delete I64;
  delete I128;
}

TEST(SmallIndexMap, ClassWithDefaultCtor)
{
  struct S
  {
    int I;
    S() : I(0) {}
    S(int I) : I(I) {}
  };

  SmallIndexMap<S, 4, /* StoreInPlace =*/true> M;
  ASSERT_EQ(M.size(), 0);
  ASSERT_TRUE(M.isSmall());
  M[0] = 1;
  M[1] = 2;
  M[2] = 3;
  M[3] = 4;
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isSmall());

  EXPECT_EQ(M.get(0).I, 1);
  EXPECT_EQ(M.get(1).I, 2);
  EXPECT_EQ(M.get(2).I, 3);
  EXPECT_EQ(M.get(3).I, 4);

  S* SPtrOrig = M.tryGet(3);

  M[32] = 64;
  M[64] = 128;
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  S* SPtrAfterLarge = M.tryGet(3);
  // In-place storage is not reference-stable.
  EXPECT_NE(SPtrOrig, SPtrAfterLarge);

  M.erase(0);
  M.erase(1);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isLarge());

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(2), nullptr);
  EXPECT_EQ(M.tryGet(2)->I, 3);

  M.erase(2);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  S* SPtrAfterSmall = M.tryGet(3);
  EXPECT_NE(SPtrAfterLarge, SPtrAfterSmall);
}

TEST(SmallIndexMap, ClassWithDefaultCtorStoreOnHeap)
{
  struct S
  {
    int I;
    S() : I(0) {}
    S(int I) : I(I) {}
  };

  SmallIndexMap<S, 4, /* StoreInPlace =*/false> M;
  ASSERT_EQ(M.size(), 0);
  ASSERT_TRUE(M.isSmall());
  M[0] = 1;
  M[1] = 2;
  M[2] = 3;
  M[3] = 4;
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isSmall());

  EXPECT_EQ(M.get(0).I, 1);
  EXPECT_EQ(M.get(1).I, 2);
  EXPECT_EQ(M.get(2).I, 3);
  EXPECT_EQ(M.get(3).I, 4);

  S* SPtrOrig = M.tryGet(3);

  M[32] = 64;
  M[64] = 128;
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  S* SPtrAfterLarge = M.tryGet(3);
  // Out-of-place storage is reference-stable.
  EXPECT_EQ(SPtrOrig, SPtrAfterLarge);

  M.erase(0);
  M.erase(1);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isLarge());

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(2), nullptr);
  EXPECT_EQ(M.tryGet(2)->I, 3);

  M.erase(2);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  S* SPtrAfterSmall = M.tryGet(3);
  EXPECT_EQ(SPtrAfterLarge, SPtrAfterSmall);
}

TEST(SmallIndexMap, ClassNoDefaultCtor)
{
  struct S
  {
    int I;
    S(int I) : I(I) {}
  };

  SmallIndexMap<S, 4, /* StoreInPlace =*/true> M;
  ASSERT_EQ(M.size(), 0);
  ASSERT_TRUE(M.isSmall());
  M.set(0, 1);
  M.set(1, 2);
  M.set(2, 3);
  M.set(3, 4);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isSmall());

  EXPECT_EQ(M.get(0).I, 1);
  EXPECT_EQ(M.get(1).I, 2);
  EXPECT_EQ(M.get(2).I, 3);
  EXPECT_EQ(M.get(3).I, 4);

  S* SPtrOrig = M.tryGet(3);

  M.set(32, 64);
  M.set(64, 128);
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  S* SPtrAfterLarge = M.tryGet(3);
  // In-place storage is not reference-stable.
  EXPECT_NE(SPtrOrig, SPtrAfterLarge);

  M.erase(0);
  M.erase(1);
  ASSERT_EQ(M.size(), 4);
  ASSERT_TRUE(M.isLarge());

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(2), nullptr);
  EXPECT_EQ(M.tryGet(2)->I, 3);

  M.erase(2);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  S* SPtrAfterSmall = M.tryGet(3);
  EXPECT_NE(SPtrAfterLarge, SPtrAfterSmall);
}

TEST(SmallIndexMap, LargeMap)
{
  struct S
  {
    int I;
    S() : I(0) {}
    S(int I) : I(I) {}
  };

  SmallIndexMap<S, 4096, /* StoreInPlace =*/false> M;
  EXPECT_EQ(M.size(), 0);
  EXPECT_TRUE(M.isSmall());

  M[4096] = -4096;
  EXPECT_EQ(M.size(), 1);
  EXPECT_TRUE(M.isLarge());

  for (std::size_t I = 0; I < 4096; ++I)
    M[I] = -I;
  EXPECT_EQ(M.size(), 4096 + 1);
  EXPECT_TRUE(M.isLarge());

  M[8192] = -8192;
  EXPECT_EQ(M.size(), 4096 + 1 + 1);
  EXPECT_TRUE(M.isLarge());

  M[2048] = -20480; // Overwriting existing element!
  EXPECT_EQ(M.size(), 4096 + 1 + 1);
  EXPECT_TRUE(M.isLarge());

  M.erase(8192);
  EXPECT_EQ(M.size(), 4096 + 1);
  EXPECT_TRUE(M.isLarge());

  for (std::size_t I = 0; I < 4096 / 2; ++I)
    M.erase(I);
  EXPECT_EQ(M.size(), 4096 / 2 + 1);
  EXPECT_TRUE(M.isLarge());

  M.erase(4096);
  EXPECT_EQ(M.size(), 4096 / 2);
  EXPECT_TRUE(M.isSmall());
}
