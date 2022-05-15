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
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "monomux/adt/MemberFunctionHelper.hpp"
#include "monomux/adt/UniqueScalar.hpp"

namespace monomux
{

/// A ring buffer based backing store that can contain an arbitrary count of
/// objects of a type.
///
/// Given an origin point (the logical \e Begin) and an \e End point, the
/// values in the ring buffer between \e Begin and \e End are valid, but this
/// range may overlap the physical end of the buffer, in which case subsequent
/// elements are allocated from the physical beginning.
///
///   \code
///
///      +--------+--------+--------+--------+--------+--------+--------+
///      | elem 4 |        |        | elem 0 | elem 1 | elem 2 | elem 3 |
///      +--------+--------+--------+--------+--------+--------+--------+
///
///      ^        ^                 ^                                   ^
///      |        \- Logical end    |                                   |
///      \- Physical begin          \- Logical Begin     Physical end  -/
///
///   \endcode
///
/// Unlike queues on sequential containers, reading from the ring does \b NOT
/// move elements to the left to position them at "start". This comes with its
/// drawbacks, however: an access of a particular length into the storage may
/// simply not yield a contiguous buffer, depending on the "origin point" of the
/// ring.
///
/// For these reasons, this implementation only supports pushing and popping
/// from the backing data structure.
///
/// \tparam T The element type to store. Ring storage works best if T is
/// default-constructible and this construction is cheap.
template <class T> class RingStorage
{
  using StorageType = std::unique_ptr<T[]>;

  static constexpr bool NothrowAssignable = std::is_nothrow_assignable_v<T, T>;

public:
  RingStorage(std::size_t InitialSize)
    : Storage(new T[InitialSize]), Capacity(InitialSize),
      Origin(physicalBegin()), End(physicalBegin())
  {}

  RingStorage(std::initializer_list<T> Init) : RingStorage(Init.size())
  {
    for (auto& E : Init)
      emplace_back(std::move(E));
  }

  std::size_t capacity() const noexcept { return Capacity; }
  std::size_t size() const noexcept { return Size; }
  bool empty() const noexcept { return Size == 0; }

  /// Copies the element \p V to the end of the buffer.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void push_back(const T& V) noexcept(NothrowAssignable) { emplace_back(V); }

  /// Constructs and then move-assigns the element type \t P at the end of
  /// the buffer from the specified \p Args.
  template <typename... Args>
  // NOLINTNEXTLINE(readability-identifier-naming)
  void emplace_back(Args&&... Argv) noexcept(
    NothrowAssignable&& std::is_nothrow_constructible_v<T, Args...>)
  {
    if (Size == Capacity)
      grow();

    T* InsertPos = nextSlot();
    *InsertPos = T{std::forward<Args>(Argv)...};

    End = InsertPos + 1;
    ++Size;
  }

  /// Copies the element \p V to the beginning of the buffer.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void push_front(const T& V) noexcept(NothrowAssignable) { emplace_front(V); }

  /// Constructs and then move-assigns the element type \t P at the beginning of
  /// the buffer from the specified \p Args.
  template <typename... Args>
  // NOLINTNEXTLINE(readability-identifier-naming)
  void emplace_front(Args&&... Argv) noexcept(
    NothrowAssignable&& std::is_nothrow_constructible_v<T, Args...>)
  {
    if (Size == Capacity)
      grow();

    T* InsertPos = prevSlot();
    *InsertPos = T{std::forward<Args>(Argv)...};

    Origin = InsertPos;
    ++Size;
  }

  /// \returns a reference to the element at the specified location \p Index.
  const T& at(std::size_t Index) const
  {
    if (Index >= Size)
      throw std::out_of_range{std::string{"idx "} + std::to_string(Index) +
                              " >= size " + std::to_string(Size)};

    return *translateIndex(Index);
  }
  /// \returns a reference to the element at the specified location \p Index.
  MEMBER_FN_NON_CONST_1(T&, at, std::size_t, Index);
  /// \returns a reference to the element at the specified location \p Index.
  const T& operator[](std::size_t Index) const { return at(Index); }
  /// \returns a reference to the element at the specified location \p Index.
  T& operator[](std::size_t Index) { return at(Index); }

  void clear() noexcept(NothrowAssignable)
  {
    for (std::size_t I = 0; I < Size; ++I)
      *translateIndex(I) = T{};
    Size = 0;
    resetToPhysicalOriginIfEmpty();
  }

  /// \returns a reference to the first element.
  const T& front() const
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};
    return at(0);
  }
  /// \returns a reference to the first element.
  MEMBER_FN_NON_CONST_0(T&, front);
  /// \returns a reference to the last element.
  const T& back() const
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};
    return at(Size - 1);
  }
  /// \returns a reference to the last element.
  MEMBER_FN_NON_CONST_0(T&, back);

  /// Removes the first element (\p front()) from the buffer.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void pop_front() noexcept(NothrowAssignable)
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};

    *Origin = T{};

    ++Origin;
    if (Origin == physicalEnd())
      Origin = physicalBegin();
    --Size;

    resetToPhysicalOriginIfEmpty();
  }
  /// Removes the first element (\p front()) from the buffer.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void pop_back() noexcept(NothrowAssignable)
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};

    *translateIndex(Size - 1) = T{};

    if (End == physicalBegin())
      End = physicalEnd();
    else
      --End;
    --Size;

    resetToPhysicalOriginIfEmpty();
  }

  /// Consume at most \p N elements from the beginning of the buffer.
  ///
  /// \see dropFront, peekFront
  std::vector<T> takeFront(std::size_t N)
  {
    std::vector<T> V = peekFront(N);
    dropFront(V.size());
    return V;
  }

  /// Discard at most \p N elements from the beginning of the buffer.
  ///
  /// \see takeFront, peekFront
  void dropFront(std::size_t N)
  {
    if (N > Size)
      Size = N;

    Origin = translateIndex(N);
    Size -= N;

    resetToPhysicalOriginIfEmpty();
  }

  /// Copy out at most \p N elements from the beginning of the buffer, but
  /// do not consume it from the buffer.
  ///
  /// \see takeFront, dropFront
  std::vector<T> peekFront(std::size_t N)
  {
    if (N > Size)
      N = Size;

    std::vector<T> V;
    V.reserve(N);

    T* P = Origin;
    while (N > 0)
    {
      V.emplace_back(std::move(*P));
      ++P;
      --N;

      if (P == physicalEnd())
        P = physicalBegin();
    }

    return V;
  }

  /// Push the contents of \p V to the end of the buffer.
  void putBack(std::vector<T> V) { putBack(V.data(), V.size()); }

  /// Push \p N elements starting at \p Ptr to the end of the buffer.
  void putBack(T* Ptr, std::size_t N)
  {
    if (Size + N > Capacity)
      grow(Size + N);

    std::size_t C = 0;
    T* P = nextSlot();
    while (C != N)
    {
      *P = std::move(*Ptr);

      ++Ptr;
      ++P;
      ++C;

      if (P == physicalEnd())
        P = physicalBegin();
    }

    Size += N;
    End = P;
  }

  /// Push \p N elements starting at \p Ptr to the end of the buffer.
  void putBack(const T* Ptr, std::size_t N)
  {
    if (Size + N > Capacity)
      grow(Size + N);

    std::size_t C = 0;
    T* P = nextSlot();
    while (C != N)
    {
      *P = *Ptr;

      ++Ptr;
      ++P;
      ++C;

      if (P == physicalEnd())
        P = physicalBegin();
    }

    Size += N;
    End = P;
  }

private:
  StorageType Storage;
  /// The physical size of the allocated buffer.
  std::size_t Capacity;
  /// The number of elements mapped.
  std::size_t Size = 0;

  T* physicalBegin() const noexcept { return Storage.get(); }
  T* physicalEnd() const noexcept { return (Storage.get()) + Capacity; }

  /// The \p Origin represents the logical \e Begin point of the buffer, the
  /// place where the first element is (if there are any).
  UniqueScalar<T*, nullptr> Origin;
  /// The \p End represents the logical \e End point of the buffer. This points
  /// to the empty space after the last actually placed elements.
  UniqueScalar<T*, nullptr> End;

  /// \returns the location where a back-insertion into the buffer should take
  /// place, or \p nullptr if the buffer is full.
  T* nextSlot() noexcept
  {
    T* P = End;
    if (P >= physicalEnd())
      // If we are over the physical end, start again from the beginning.
      P = physicalBegin();
    if (Size != 0 && P == Origin)
      // The buffer is full.
      return nullptr;
    return P;
  }

  /// \returns the location where a front-insertion into the buffer should take
  /// place, or \p nullptr if the buffer is full.
  T* prevSlot() noexcept
  {
    T* P = Origin;
    if (P == physicalBegin())
      // We begun at the physical begin, so the slot we want is the one
      // right before the end. However, the buffer may be full if the physical
      // and logical ends meet up.
      P = physicalEnd();
    if (Size != 0 && P == End)
      // The buffer is full.
      return nullptr;

    --P; // Go the previous location.
    return P;
  }

  /// \returns the pointer that points to the \p Ith logical element in the
  /// storage. The element at this location may or may not be valid!
  T* translateIndex(std::size_t I) const noexcept
  {
    return physicalBegin() + (((Origin - physicalBegin()) + I) % Capacity);
  }

  void resetToPhysicalOriginIfEmpty()
  {
    if (!empty())
      return;

    Origin.get() = End.get() = physicalBegin();
  }

  /// Compacts the elements to the left side of the buffer, making the physical
  /// begin and the logical begin start at the beginning of the array.
  void rotateToPhysical()
  {
    T* B = physicalBegin();
    T* E = physicalEnd();
    std::rotate(B, Origin.get(), E);
    Origin = B;
    E = B + Size;
  }

  /// Grows the size of the buffer to allow for twice as many elements as
  /// before, or at least \p NewCapacityAtLeast elements if the parameter is
  /// non-zero.
  void grow(std::size_t NewCapacityAtLeast = 0)
  {
    rotateToPhysical();

    std::size_t NewCapacity = Capacity;
    if (NewCapacityAtLeast == 0 || NewCapacityAtLeast < Capacity)
      NewCapacity = Capacity * 2;
    else
      while (NewCapacity < NewCapacityAtLeast)
        NewCapacity *= 2;

    StorageType New{new T[NewCapacity]};
    for (std::size_t I = 0; I < Size; ++I)
      New[I] = std::move(Storage[I]);
    std::swap(Storage, New);

    Capacity = NewCapacity;
    Origin = physicalBegin();
    End = Origin + Size;
  }
};

} // namespace monomux
