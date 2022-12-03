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

#include "monomux/adt/RingBuffer.hpp"

// NOLINTBEGIN(cert-err58-cpp)

using namespace monomux;

static constexpr int Magic32 = 32;
static constexpr int Magic64 = 64;

TEST(RingBuffer, CreateInsertAccess)
{
  RingBuffer<int> RB(static_cast<std::size_t>(4));
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 0);

  RB.push_back(1);
  RB.emplace_back(2);
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 2);

  EXPECT_EQ(RB.at(0), 1);
  EXPECT_EQ(RB.at(1), 2);
  EXPECT_THROW(RB.at(2), std::out_of_range);
  EXPECT_THROW(RB[3], std::out_of_range);

  RB.emplace_back(3);
  RB.emplace_back(4);
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 4);
  EXPECT_EQ(RB[2], 3);
  EXPECT_EQ(RB[3], 4);

  EXPECT_EQ(RB.front(), 1);
  EXPECT_EQ(RB.back(), 4);
}

TEST(RingBuffer, Clear)
{
  RingBuffer<int> RB = {1, 2, 3, 4};
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 4);
  EXPECT_EQ(RB[0], 1);
  EXPECT_EQ(RB[1], 2);
  EXPECT_EQ(RB[2], 3);
  EXPECT_EQ(RB[3], 4);

  RB.clear();
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 0);
  EXPECT_THROW(RB[0], std::out_of_range);
}

TEST(RingBuffer, PushPop)
{
  RingBuffer<int> RB = {1, 2, 3, 4};
  // [*1, 2, 3, 4]
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 4);

  EXPECT_EQ(RB.front(), 1);
  EXPECT_EQ(RB.back(), 4);
  RB.pop_front();
  // [-, *2, 3, 4]
  EXPECT_EQ(RB.front(), 2);
  EXPECT_EQ(RB.back(), 4);
  RB.pop_front();
  // [-, -, *3, 4]
  EXPECT_EQ(RB.back(), 4);

  EXPECT_EQ(RB.size(), 2);

  RB.push_front(Magic32);
  EXPECT_EQ(RB.size(), 3);
  EXPECT_EQ(RB.front(), Magic32);
  EXPECT_EQ(RB.back(), 4);
  // [-, *32, 3, 4]

  RB.pop_front();
  // [-, -, *3, 4]
  RB.pop_back();
  // [-, -, *3, -]
  EXPECT_EQ(RB.size(), 1);
  EXPECT_EQ(RB.front(), 3);
  EXPECT_EQ(RB.back(), 3);

  RB.push_back(Magic32);
  // [-, -, *3, 32]
  EXPECT_EQ(RB.size(), 2);
  EXPECT_EQ(RB.front(), 3);
  EXPECT_EQ(RB.back(), Magic32);

  RB.push_back(Magic64);
  // [64, -, *3, 32]
  EXPECT_EQ(RB.size(), 3);
  EXPECT_EQ(RB.front(), 3);
  EXPECT_EQ(RB.back(), Magic64);

  RB.pop_front();
  // [64, -, -, *32]
  EXPECT_EQ(RB.size(), 2);
  EXPECT_EQ(RB.front(), Magic32);
  EXPECT_EQ(RB.back(), Magic64);

  RB.pop_front();
  // [*64, -, -, -]
  EXPECT_EQ(RB.size(), 1);
  EXPECT_EQ(RB.front(), Magic64);
  EXPECT_EQ(RB.back(), Magic64);

  RB.pop_front();
  // [-, -, -, -]
  EXPECT_EQ(RB.size(), 0);
  EXPECT_THROW(RB.front(), std::out_of_range);
}

TEST(RingBuffer, Grow)
{
  RingBuffer<int> RB = {1, 2};
  EXPECT_EQ(RB.capacity(), 2);
  EXPECT_EQ(RB.size(), 2);

  RB.push_back(3);
  EXPECT_EQ(RB.capacity(), 4);
  EXPECT_EQ(RB.size(), 3);
}

TEST(RingBuffer, PutAndTake)
{
  RingBuffer<int> RB(static_cast<std::size_t>(4));
  EXPECT_EQ(RB.capacity(), 4);

  RB.putBack({1, 2, 3, 4});
  EXPECT_EQ(RB.size(), 4);
  auto V = RB.takeFront(4);
  EXPECT_EQ(RB.size(), 0);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(V[3], 4);

  RB.putBack({1, 2, 1, 2, 1, 2, 1, 2, 1, 2});
  // [*1, 2, 1, 1, 1, 2, 1, 2, 1, 2, -, -, -, -, -, -]
  EXPECT_EQ(RB.capacity(),
            16); // Grows to double the size every time, 4 * 2 * 2.
  EXPECT_EQ(RB.size(), 10);

  V = RB.takeFront(4);
  // [-, -, -, -, *1, 2, 1, 2, 1, 2, -, -, -, -, -, -]
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 1);
  EXPECT_EQ(V[3], 2);
  EXPECT_EQ(RB.size(), 6);

  RB.putBack({3, 4, 3, 4, 3, 4});
  // [-, -, -, -, *1, 2, 1, 2, 1, 2, 3, 4, 3, 4, 3, 4]
  EXPECT_EQ(RB.capacity(), 16);
  EXPECT_EQ(RB.size(), 12);
  EXPECT_EQ(RB[5], 2);
  EXPECT_EQ(RB[6], 3);
  EXPECT_EQ(RB[7], 4);
  EXPECT_EQ(RB[10], 3);
  EXPECT_EQ(RB[11], 4);
  EXPECT_EQ(RB.front(), 1);
  EXPECT_EQ(RB.back(), 4);

  RB.putBack({Magic32, Magic64, Magic32, Magic64});
  // [32, 64, 32, 64, *1, 2, 1, 2, 1, 2, 3, 4, 3, 4, 3, 4]
  EXPECT_EQ(RB.capacity(), 16);
  EXPECT_EQ(RB.size(), 16);
  EXPECT_EQ(RB[0], 1);
  EXPECT_EQ(RB[1], 2);
  EXPECT_EQ(RB[10], 3);
  EXPECT_EQ(RB[11], 4);
  EXPECT_EQ(RB[14], Magic32);
  EXPECT_EQ(RB[15], Magic64);

  V = RB.takeFront(6); // NOLINT(readability-magic-numbers)
  // [32, 64, 32, 64, -, -, -, -, -, -, *3, 4, 3, 4, 3, 4]
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 1);
  EXPECT_EQ(V[3], 2);
  EXPECT_EQ(V[4], 1);
  EXPECT_EQ(V[5], 2);
  EXPECT_EQ(RB.size(), 6 + 4);

  RB.putBack({0, -1, 0, -1, 0, -1, 0, -1});
  // [*3, 4, 3, 4, 3, 4, 32, 64, 32, 64, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -,
  // ...]
  EXPECT_EQ(RB.capacity(), 32); // Grows to double the size every time, 16 * 2.
  EXPECT_EQ(RB.size(), 6 + 4 + 8);
  EXPECT_EQ(RB[0], 3);
  EXPECT_EQ(RB[1], 4);
  EXPECT_EQ(RB[6], Magic32);
  EXPECT_EQ(RB[7], Magic64);
  EXPECT_EQ(RB[10], 0);
  EXPECT_EQ(RB[11], -1);
}

TEST(RingBuffer, PeekAndDrop)
{
  RingBuffer<int> RB = {1, 2, 3, 4, 1, 2, 3, 4};
  EXPECT_EQ(RB.capacity(), 8);
  EXPECT_EQ(RB.size(), 8);

  std::vector<int> V = RB.peekFront(3);
  // [*1, 2, 3, 4, 1, 2, 3, 4]
  EXPECT_EQ(RB.size(), 8);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(RB.front(), 1);

  V = RB.takeFront(3);
  // [-, -, -, *4, 1, 2, 3, 4]
  EXPECT_EQ(RB.size(), 5);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(RB.front(), 4);
  EXPECT_EQ(RB[1], 1);
  EXPECT_EQ(RB[2], 2);

  RB.dropFront(3);
  // [-, -, -, -, -, -, *3, 4]
  EXPECT_EQ(RB.size(), 2);
  EXPECT_EQ(RB[0], 3);
  EXPECT_EQ(RB[1], 4);
}

// NOLINTEND(cert-err58-cpp)
