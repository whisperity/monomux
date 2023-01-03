/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>

#include "monomux/adt/SmallIndexMap.hpp"

// NOLINTBEGIN(cert-err58-cpp)

using namespace monomux;

static constexpr int Magic32 = 32;
static constexpr int Magic64 = 64;
static constexpr int Magic128 = 128;
static constexpr int Magic256 = 256;
static constexpr int Magic2048 = 2048;
static constexpr int Magic4096 = 4096;
static constexpr int Magic8192 = 8192;

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

  M[Magic32] = Magic64;
  M[Magic64] = Magic128;
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

  M.erase(Magic256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());
}

TEST(SmallIndexMap, Clear)
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

  M[Magic32] = Magic64;
  M[Magic64] = Magic128;
  ASSERT_EQ(M.size(), 6);
  ASSERT_TRUE(M.isLarge());

  M.clear();

  EXPECT_EQ(M.tryGet(0), nullptr);
  EXPECT_EQ(M.tryGet(1), nullptr);
  EXPECT_EQ(M.tryGet(2), nullptr);
  EXPECT_EQ(M.tryGet(3), nullptr);
  EXPECT_EQ(M.tryGet(Magic32), nullptr);
  EXPECT_EQ(M.tryGet(Magic64), nullptr);
  ASSERT_EQ(M.size(), 0);
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

  int* I64 = new int(Magic64);
  int* I128 = new int(Magic128);

  M[Magic32] = I64;
  M[Magic64] = I128;
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

  M.erase(Magic256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic64);
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
    int I{};
    S() = default;
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

  M[Magic32] = Magic64;
  M[Magic64] = Magic128;
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

  M.erase(Magic256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  S* SPtrAfterSmall = M.tryGet(3);
  EXPECT_NE(SPtrAfterLarge, SPtrAfterSmall);
}

TEST(SmallIndexMap, ClassWithDefaultCtorStoreOnHeap)
{
  struct S
  {
    int I{};
    S() = default;
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

  M[Magic32] = Magic64;
  M[Magic64] = Magic128;
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

  M.erase(Magic256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic64);
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

  M.set(Magic32, Magic64);
  M.set(Magic64, Magic128);
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

  M.erase(Magic256);
  ASSERT_EQ(M.size(), 3);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic32);
  // Even though size 2 would allow going back to small size, the index does
  // not.
  ASSERT_EQ(M.size(), 2);
  ASSERT_TRUE(M.isLarge());

  M.erase(Magic64);
  ASSERT_EQ(M.size(), 1);
  ASSERT_TRUE(M.isSmall());

  S* SPtrAfterSmall = M.tryGet(3);
  EXPECT_NE(SPtrAfterLarge, SPtrAfterSmall);
}

TEST(SmallIndexMap, IntrusiveDefaultSentinel)
{
  struct S
  {
    int I{};
    S() = default;
    S(int I) : I(I) {}

    // Sentinel check requires equality.
    bool operator==(const S& RHS) const { return I == RHS.I; };
    bool operator!=(const S& RHS) const { return !(*this == RHS); };
  };

  SmallIndexMap<S,
                4,
                /* StoreInPlace =*/true,
                /* IntrusiveDefaultSentinel =*/true>
    M;

  M[0] = 1;
  M[1] = 2;
  M[2] = S{}; // Sentinel collision.
  M[3];

  EXPECT_EQ(M.size(), 4);
  EXPECT_NE(M.tryGet(0), nullptr);
  EXPECT_NE(M.tryGet(1), nullptr);
  EXPECT_EQ(M.tryGet(2), nullptr); // Sentinel element considered as not mapped.
  EXPECT_EQ(M.tryGet(3), nullptr); // Sentinel element considered as not mapped.
}

TEST(SmallIndexMap, LargeMap)
{
  struct S
  {
    int I{};
    S() = default;
    S(int I) : I(I) {}
  };

  SmallIndexMap<S, Magic4096, /* StoreInPlace =*/false> M;
  EXPECT_EQ(M.size(), 0);
  EXPECT_TRUE(M.isSmall());

  M[Magic4096] = -Magic4096;
  EXPECT_EQ(M.size(), 1);
  EXPECT_TRUE(M.isLarge());

  for (std::size_t I = 0; I < Magic4096; ++I)
    M[I] = -I;
  EXPECT_EQ(M.size(), 4096 + 1);
  EXPECT_TRUE(M.isLarge());

  M[Magic8192] = -Magic8192;
  EXPECT_EQ(M.size(), 4096 + 1 + 1);
  EXPECT_TRUE(M.isLarge());

  M[Magic2048] = -Magic2048; // Overwriting existing element!
  EXPECT_EQ(M.size(), 4096 + 1 + 1);
  EXPECT_TRUE(M.isLarge());

  M.erase(Magic8192);
  EXPECT_EQ(M.size(), 4096 + 1);
  EXPECT_TRUE(M.isLarge());

  for (std::size_t I = 0; I < Magic4096 / 2; ++I)
    M.erase(I);
  EXPECT_EQ(M.size(), 4096 / 2 + 1);
  EXPECT_TRUE(M.isLarge());

  M.erase(Magic4096);
  EXPECT_EQ(M.size(), 4096 / 2);
  EXPECT_TRUE(M.isSmall());
}

// NOLINTEND(cert-err58-cpp)
