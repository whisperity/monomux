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

#include "monomux/adt/RingStorage.hpp"

using namespace monomux;

static constexpr int Magic32 = 32;
static constexpr int Magic64 = 64;

TEST(RingStorage, CreateInsertAccess)
{
  RingStorage<int> RS(static_cast<std::size_t>(4));
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 0);

  RS.push_back(1);
  RS.emplace_back(2);
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 2);

  EXPECT_EQ(RS.at(0), 1);
  EXPECT_EQ(RS.at(1), 2);
  EXPECT_THROW(RS.at(2), std::out_of_range);
  EXPECT_THROW(RS[3], std::out_of_range);

  RS.emplace_back(3);
  RS.emplace_back(4);
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 4);
  EXPECT_EQ(RS[2], 3);
  EXPECT_EQ(RS[3], 4);

  EXPECT_EQ(RS.front(), 1);
  EXPECT_EQ(RS.back(), 4);
}

TEST(RingStorage, Clear)
{
  RingStorage<int> RS = {1, 2, 3, 4};
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 4);
  EXPECT_EQ(RS[0], 1);
  EXPECT_EQ(RS[1], 2);
  EXPECT_EQ(RS[2], 3);
  EXPECT_EQ(RS[3], 4);

  RS.clear();
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 0);
  EXPECT_THROW(RS[0], std::out_of_range);
}

TEST(RingStorage, PushPop)
{
  RingStorage<int> RS = {1, 2, 3, 4};
  // [*1, 2, 3, 4]
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 4);

  EXPECT_EQ(RS.front(), 1);
  EXPECT_EQ(RS.back(), 4);
  RS.pop_front();
  // [-, *2, 3, 4]
  EXPECT_EQ(RS.front(), 2);
  EXPECT_EQ(RS.back(), 4);
  RS.pop_front();
  // [-, -, *3, 4]
  EXPECT_EQ(RS.back(), 4);

  EXPECT_EQ(RS.size(), 2);

  RS.push_front(Magic32);
  EXPECT_EQ(RS.size(), 3);
  EXPECT_EQ(RS.front(), Magic32);
  EXPECT_EQ(RS.back(), 4);
  // [-, *32, 3, 4]

  RS.pop_front();
  // [-, -, *3, 4]
  RS.pop_back();
  // [-, -, *3, -]
  EXPECT_EQ(RS.size(), 1);
  EXPECT_EQ(RS.front(), 3);
  EXPECT_EQ(RS.back(), 3);

  RS.push_back(Magic32);
  // [-, -, *3, 32]
  EXPECT_EQ(RS.size(), 2);
  EXPECT_EQ(RS.front(), 3);
  EXPECT_EQ(RS.back(), Magic32);

  RS.push_back(Magic64);
  // [64, -, *3, 32]
  EXPECT_EQ(RS.size(), 3);
  EXPECT_EQ(RS.front(), 3);
  EXPECT_EQ(RS.back(), Magic64);

  RS.pop_front();
  // [64, -, -, *32]
  EXPECT_EQ(RS.size(), 2);
  EXPECT_EQ(RS.front(), Magic32);
  EXPECT_EQ(RS.back(), Magic64);

  RS.pop_front();
  // [*64, -, -, -]
  EXPECT_EQ(RS.size(), 1);
  EXPECT_EQ(RS.front(), Magic64);
  EXPECT_EQ(RS.back(), Magic64);

  RS.pop_front();
  // [-, -, -, -]
  EXPECT_EQ(RS.size(), 0);
  EXPECT_THROW(RS.front(), std::out_of_range);
}

TEST(RingStorage, Grow)
{
  RingStorage<int> RS = {1, 2};
  EXPECT_EQ(RS.capacity(), 2);
  EXPECT_EQ(RS.size(), 2);

  RS.push_back(3);
  EXPECT_EQ(RS.capacity(), 4);
  EXPECT_EQ(RS.size(), 3);
}

TEST(RingStorage, PutAndTake)
{
  RingStorage<int> RS(static_cast<std::size_t>(4));
  EXPECT_EQ(RS.capacity(), 4);

  RS.putBack({1, 2, 3, 4});
  EXPECT_EQ(RS.size(), 4);
  auto V = RS.takeFront(4);
  EXPECT_EQ(RS.size(), 0);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(V[3], 4);

  RS.putBack({1, 2, 1, 2, 1, 2, 1, 2, 1, 2});
  // [*1, 2, 1, 1, 1, 2, 1, 2, 1, 2, -, -, -, -, -, -]
  EXPECT_EQ(RS.capacity(),
            16); // Grows to double the size every time, 4 * 2 * 2.
  EXPECT_EQ(RS.size(), 10);

  V = RS.takeFront(4);
  // [-, -, -, -, *1, 2, 1, 2, 1, 2, -, -, -, -, -, -]
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 1);
  EXPECT_EQ(V[3], 2);
  EXPECT_EQ(RS.size(), 6);

  RS.putBack({3, 4, 3, 4, 3, 4});
  // [-, -, -, -, *1, 2, 1, 2, 1, 2, 3, 4, 3, 4, 3, 4]
  EXPECT_EQ(RS.capacity(), 16);
  EXPECT_EQ(RS.size(), 12);
  EXPECT_EQ(RS[5], 2);
  EXPECT_EQ(RS[6], 3);
  EXPECT_EQ(RS[7], 4);
  EXPECT_EQ(RS[10], 3);
  EXPECT_EQ(RS[11], 4);
  EXPECT_EQ(RS.front(), 1);
  EXPECT_EQ(RS.back(), 4);

  RS.putBack({Magic32, Magic64, Magic32, Magic64});
  // [32, 64, 32, 64, *1, 2, 1, 2, 1, 2, 3, 4, 3, 4, 3, 4]
  EXPECT_EQ(RS.capacity(), 16);
  EXPECT_EQ(RS.size(), 16);
  EXPECT_EQ(RS[0], 1);
  EXPECT_EQ(RS[1], 2);
  EXPECT_EQ(RS[10], 3);
  EXPECT_EQ(RS[11], 4);
  EXPECT_EQ(RS[14], Magic32);
  EXPECT_EQ(RS[15], Magic64);

  V = RS.takeFront(6); // NOLINT(readability-magic-numbers)
  // [32, 64, 32, 64, -, -, -, -, -, -, *3, 4, 3, 4, 3, 4]
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 1);
  EXPECT_EQ(V[3], 2);
  EXPECT_EQ(V[4], 1);
  EXPECT_EQ(V[5], 2);
  EXPECT_EQ(RS.size(), 6 + 4);

  RS.putBack({0, -1, 0, -1, 0, -1, 0, -1});
  // [*3, 4, 3, 4, 3, 4, 32, 64, 32, 64, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, -,
  // ...]
  EXPECT_EQ(RS.capacity(), 32); // Grows to double the size every time, 16 * 2.
  EXPECT_EQ(RS.size(), 6 + 4 + 8);
  EXPECT_EQ(RS[0], 3);
  EXPECT_EQ(RS[1], 4);
  EXPECT_EQ(RS[6], Magic32);
  EXPECT_EQ(RS[7], Magic64);
  EXPECT_EQ(RS[10], 0);
  EXPECT_EQ(RS[11], -1);
}

TEST(RingStorage, PeekAndDrop)
{
  RingStorage<int> RS = {1, 2, 3, 4, 1, 2, 3, 4};
  EXPECT_EQ(RS.capacity(), 8);
  EXPECT_EQ(RS.size(), 8);

  std::vector<int> V = RS.peekFront(3);
  // [*1, 2, 3, 4, 1, 2, 3, 4]
  EXPECT_EQ(RS.size(), 8);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(RS.front(), 1);

  V = RS.takeFront(3);
  // [-, -, -, *4, 1, 2, 3, 4]
  EXPECT_EQ(RS.size(), 5);
  EXPECT_EQ(V[0], 1);
  EXPECT_EQ(V[1], 2);
  EXPECT_EQ(V[2], 3);
  EXPECT_EQ(RS.front(), 4);
  EXPECT_EQ(RS[1], 1);
  EXPECT_EQ(RS[2], 2);

  RS.dropFront(3);
  // [-, -, -, -, -, -, *3, 4]
  EXPECT_EQ(RS.size(), 2);
  EXPECT_EQ(RS[0], 3);
  EXPECT_EQ(RS[1], 4);
}
