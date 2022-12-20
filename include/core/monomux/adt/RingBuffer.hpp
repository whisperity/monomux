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
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/UniqueScalar.hpp"

namespace monomux
{

namespace detail
{

/// Helper class that contains the details of \p RingBuffer that is not
/// dependent on the \p T template parameter, and also deals with calculating
/// optimal shrinking resizes for the buffer.
class RingBufferBase
{
  static constexpr std::size_t Kilo = 1024;

  const std::size_t OriginalCapacity;

public:
  std::size_t capacity() const noexcept { return Capacity; }
  std::size_t size() const noexcept { return Size; }
  bool empty() const noexcept { return Size == 0; }

  std::size_t originalCapacity() const noexcept { return OriginalCapacity; }
  std::chrono::time_point<std::chrono::system_clock> lastAccess() const noexcept
  {
    return LastAccess;
  }

  /// \returns the profiling data of size peaks between buffer emptying
  /// gathered.
  std::vector<std::size_t> peakStats() const
  {
    std::vector<std::size_t> Peaks = SizePeaks;
    if (CurrentPeakIndex < Peaks.size() - 1)
      // Put the oldest measurement to the beginning of the data structure,
      // undoing the inner "ring buffer" used for the Peaks vector.
      std::rotate(
        Peaks.begin(), Peaks.begin() + CurrentPeakIndex + 1, Peaks.end());
    return Peaks;
  }

protected:
  /// The physical size of the allocated buffer.
  std::size_t Capacity = 0;
  /// The number of elements mapped.
  std::size_t Size = 0;

  RingBufferBase(std::size_t Capacity)
    : OriginalCapacity(Capacity), Capacity(Capacity)
  {
    markAccess();
    SizePeaks.resize(((Capacity / Kilo) + 2) * 1);
  }

  void incSize() noexcept
  {
    ++Size;
    markAccess();
    mayBePeak();
  }
  void decSize() noexcept
  {
    assert(Size != 0);
    --Size;
    markAccess();
    mayBeValley();
  }
  void addSize(std::size_t N) noexcept
  {
    Size += N;
    markAccess();
    mayBePeak();
  }
  void subSize(std::size_t N) noexcept
  {
    assert(N <= Size);
    Size -= N;
    markAccess();
    mayBeValley();
  }
  void zeroSize() noexcept
  {
    Size = 0;
    mayBeValley();
  }

  /// \returns whether the \p RingBuffer should shrink itself back to the
  /// \p originalCapacity() because most of the recent buffer uses did not
  /// exceed it meaningfully.
  bool shouldShrink() const noexcept
  {
    if (Capacity <= OriginalCapacity)
      return false;

    static constexpr std::size_t TimeThresholdSeconds = 60;
    if (std::chrono::system_clock::now() - LastAccess >=
        std::chrono::seconds(TimeThresholdSeconds))
      // Consider the buffer for shrinking if operations were successful without
      // accessing the buffer for a sufficient amount of time.
      return true;

    // Otherwise, if the buffer is continously used for a sufficient amount of
    // time, consider shrinking it in case at least half of the recent peaks
    // (inbetween each valley, i.e. zeroing the size) would've fit into the
    // original buffer capacity too.
    std::size_t ZeroPeaks = 0;
    std::size_t SufficientlySmallPeaks = 0;
    for (std::size_t Peak : SizePeaks)
      if (Peak == 0)
        ++ZeroPeaks;
      else if (Peak <= OriginalCapacity)
        ++SufficientlySmallPeaks;

    const std::size_t Threshold = (SizePeaks.size() - ZeroPeaks) / 2 + 1;
    return SufficientlySmallPeaks > Threshold;
  }

  void resetPeaks() noexcept
  {
    for (std::size_t& SPV : SizePeaks)
      SPV = 0;
    CurrentPeakIndex = 0;
    mayBePeak();
  }

private:
  /// Contains a list of maximum sizes that were observed between two zeroing
  /// points (calls to \p resetSize()).
  std::vector<std::size_t> SizePeaks;
  std::size_t CurrentPeakIndex = 0;

  std::chrono::time_point<std::chrono::system_clock> LastAccess;

  void markAccess() noexcept { LastAccess = std::chrono::system_clock::now(); }

  /// Marks the current size as the peak of the current zone, if sufficient.
  void mayBePeak() noexcept
  {
    if (Size == 0 || Capacity <= OriginalCapacity)
      return;

    if (Size > SizePeaks[CurrentPeakIndex])
      SizePeaks[CurrentPeakIndex] = Size;
  }

  /// "Commits" the previous peak and starts counting the size for the next peak
  /// after the current size has reached zero.
  void mayBeValley() noexcept
  {
    if (Size != 0 || Capacity <= OriginalCapacity)
      return;
    if (SizePeaks[CurrentPeakIndex] == 0)
      return;

    ++CurrentPeakIndex;
    if (CurrentPeakIndex >= SizePeaks.size())
      // If we reached the end of the measurement vector, restart from the
      // start. This is a little ring buffer of itself. :)
      CurrentPeakIndex = 0;
  }
};

} // namespace detail

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
template <class T> class RingBuffer : public detail::RingBufferBase
{
  using StorageType = std::unique_ptr<T[]>;
  static constexpr bool NothrowAssignable = std::is_nothrow_assignable_v<T, T>;

public:
  RingBuffer(std::size_t Capacity)
    : RingBufferBase(Capacity), StorageWithOriginalCapacity(new T[Capacity]),
      Origin(physicalBegin()), End(physicalBegin())
  {}

  RingBuffer(std::initializer_list<T> Init) : RingBuffer(Init.size())
  {
    for (auto& E : Init)
      emplace_back(std::move(E));
  }

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
    incSize();
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
    incSize();
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
  MONOMUX_MEMBER_1(T&, at, , std::size_t, Index);
  /// \returns a reference to the element at the specified location \p Index.
  const T& operator[](std::size_t Index) const { return at(Index); }
  /// \returns a reference to the element at the specified location \p Index.
  MONOMUX_MEMBER_1(T&, operator[], , std::size_t, Index);

  void clear() noexcept(NothrowAssignable)
  {
    for (std::size_t I = 0; I < originalCapacity(); ++I)
      StorageWithOriginalCapacity[I] = T{};
    zeroSize();
    tryCleanup();
  }

  /// \returns a reference to the first element.
  const T& front() const
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};
    return at(0);
  }
  /// \returns a reference to the first element.
  MONOMUX_MEMBER_0(T&, front, );
  /// \returns a reference to the last element.
  const T& back() const
  {
    if (empty())
      throw std::out_of_range{"Empty buffer."};
    return at(Size - 1);
  }
  /// \returns a reference to the last element.
  MONOMUX_MEMBER_0(T&, back, );

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
    decSize();
    tryCleanup();
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
    decSize();
    tryCleanup();
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
      N = Size;

    Origin = translateIndex(N);
    subSize(N);
    tryCleanup();
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

    addSize(N);
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

    addSize(N);
    End = P;
  }

  /// Attempts to heuristically release associated resources if the buffer was
  /// not used for a while.
  void tryCleanup()
  {
    if (!empty())
      return;

    if (shouldShrink())
      shrink(originalCapacity());
    Origin.get() = End.get() = physicalBegin();
  }

private:
  /// This storage is created at initialisation and is not freed until the
  /// destructor. It has the original capacity as intended by the client.
  StorageType StorageWithOriginalCapacity;
  /// In case the ring buffer would overflow its \b original capacity, a
  /// new storage is created here. When the buffer is \p shrink()ed, access
  /// returns to the original buffer.
  StorageType GrowingStorage;

  /// Whether the \p RingBuffer is using the growing storage or the original
  /// one.
  UniqueScalar<bool, false> UsingGrowingStorage;
  const StorageType& getStorage() const noexcept
  {
    return UsingGrowingStorage ? GrowingStorage : StorageWithOriginalCapacity;
  }
  MONOMUX_MEMBER_0(StorageType&, getStorage, );

  T* physicalBegin() const noexcept { return getStorage().get(); }
  T* physicalEnd() const noexcept { return (getStorage().get()) + Capacity; }

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

    if (NewCapacity <= Capacity)
      return;

    StorageType New{new T[NewCapacity]};
    for (std::size_t I = 0; I < Size; ++I)
      New[I] = std::move(getStorage()[I]);

    if (UsingGrowingStorage)
      std::swap(GrowingStorage, New);
    else
    {
      std::fill(StorageWithOriginalCapacity.get(),
                StorageWithOriginalCapacity.get() + originalCapacity(),
                T{});

      GrowingStorage = std::move(New);
      UsingGrowingStorage = true;
    }

    Capacity = NewCapacity;
    Origin = physicalBegin();
    End = Origin + Size;
  }

  /// Shrinks the buffer to be able to store exactly \p NewCapacity elements.
  void shrink(std::size_t NewCapacity)
  {
    assert(empty() && "shrink() when buffer was not empty!");
    if (!empty())
      return;
    if (Capacity == NewCapacity)
      return;

    resetPeaks();
    if (NewCapacity <= originalCapacity())
    {
      UsingGrowingStorage = false;
      GrowingStorage.reset();
    }
    else
    {
      UsingGrowingStorage = true;
      StorageType New{new T[NewCapacity]};
      std::swap(GrowingStorage, New);
    }

    Capacity = NewCapacity;
  }
};

} // namespace monomux
