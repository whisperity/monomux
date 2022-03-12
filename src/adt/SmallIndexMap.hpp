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

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

namespace monomux
{

/// An implementation of a \p std::map where keys are fixed to be unsigned
/// integer types. In addition, when the \p size() of the map is less than \p N,
/// lookup is small buffer optimised to be done against an \p std::array
/// instead.
///
/// \tparam StoreInPlace Whether to store the elements in-place in the backing
/// data structures. Storing elements in-place allows greater locality, but
/// makes iterators and references to the added data prone to invalidation.
/// If elements are not stored in place, access is slower due to an indirection,
/// but no operations (other than the delete of a value) of the \p SmallIndexMap
/// invalidates the reference to the mapped object.
///
/// \note If \p T is a raw pointer type, it is always stored in place.
template <typename T,
          std::size_t N,
          bool StoreInPlace = true,
          typename KeyTy = std::size_t>
class SmallIndexMap
{
  /// The threshold at which point the small representation will be re-engaged.
  static constexpr std::size_t MeaningfulSmallConversionThreshold = N / 2;

  static_assert(std::is_integral_v<KeyTy> && std::is_unsigned_v<KeyTy>,
                "Keys must be index-like for small representation to work!");

  std::size_t Size = 0;

  static constexpr bool Trivial = std::is_pointer_v<T>;
  using E = std::conditional_t<
    Trivial,
    T,
    std::conditional_t<StoreInPlace, std::optional<T>, std::unique_ptr<T>>>;

  static_assert(!Trivial || StoreInPlace,
                "If the stored value is trivial, it is automatically stored in "
                "place, and specifying StoreInPlace = false is useless.");

  static_assert(std::is_default_constructible_v<E> &&
                  std::is_move_assignable_v<E>,
                "The detail storage element type must be default constructible "
                "and moveable.");

  static constexpr bool UserTypeDefaultConstructible =
    Trivial || std::is_default_constructible_v<T>;

  using SmallRepresentation = std::array<E, N>;
  using LargeRepresentation = std::map<KeyTy, E>;
  std::variant<SmallRepresentation, LargeRepresentation> Storage;

#define NON_CONST_0(RETURN_TYPE, FUNCTION_NAME)                                \
  RETURN_TYPE FUNCTION_NAME()                                                  \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME());                        \
  }

#define NON_CONST_0_NOEXCEPT(RETURN_TYPE, FUNCTION_NAME)                       \
  RETURN_TYPE FUNCTION_NAME() noexcept                                         \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME());                        \
  }

#define NON_CONST_1(RETURN_TYPE, FUNCTION_NAME, ARG_1_TYPE, ARG_1_NAME)        \
  RETURN_TYPE FUNCTION_NAME(ARG_1_TYPE ARG_1_NAME)                             \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME(ARG_1_NAME));              \
  }

#define NON_CONST_1_NOEXCEPT(                                                  \
  RETURN_TYPE, FUNCTION_NAME, ARG_1_TYPE, ARG_1_NAME)                          \
  RETURN_TYPE FUNCTION_NAME(ARG_1_TYPE ARG_1_NAME) noexcept                    \
  {                                                                            \
    using ConstThisPtr = std::add_pointer_t<                                   \
      std::add_const_t<std::remove_pointer_t<decltype(this)>>>;                \
    return const_cast<RETURN_TYPE>(                                            \
      const_cast<ConstThisPtr>(this)->FUNCTION_NAME(ARG_1_NAME));              \
  }

public:
  /// Initialises an empty \p SmallIndexMap that starts in the small
  /// representation.
  SmallIndexMap() : Storage(SmallRepresentation{})
  {
    fillSmallRepresentation();
  }

  /// \returns Whether the data structure is currently in the small
  /// representation. In this mode, access of data is a constant operation.
  bool isSmall() const noexcept
  {
    return std::holds_alternative<SmallRepresentation>(Storage);
  }
  /// \returns Whether the data structure is currently in the large
  /// representation. In this mode, access of data is a logarithmic operation.
  bool isLarge() const noexcept { return !isSmall(); }

  /// Returns the size of the container, i.e. the number of elements added
  /// into it.
  std::size_t size() const noexcept { return Size; }

  /// Returns whether the \p Key is mapped.
  bool contains(KeyTy Key) const noexcept
  {
    if (isSmall())
    {
      if (Key >= N)
        return false;
      return isMapped(getSmallRepr()->at(Key));
    }

    return getLargeRepr()->find(Key) != getLargeRepr()->end();
  }

  /// Sets the element to one constructed by forwarding \p Args, overwriting
  /// any potential already stored element.
  ///
  /// \note This function potentially reallocates the storage buffer, and if
  /// \p StoreInPlace is \p true, invalidate \b EVERY reference and iterator
  /// to existing elements.
  template <typename... Arg> void set(KeyTy Key, Arg&&... Args)
  {
    if (isSmall())
    {
      if (Key >= N)
      {
        convertToLarge();
        return set(Key, std::forward<Arg>(Args)...);
      }

      E& Elem = getSmallRepr()->at(Key);
      if (!isMapped(Elem))
        ++Size;

      if constexpr (Trivial)
        Elem = T{std::forward<Arg>(Args)...};
      else
      {
        if constexpr (StoreInPlace)
          Elem =
            std::optional<T>{std::in_place_t{}, std::forward<Arg>(Args)...};
        else
          Elem = std::make_unique<T>(std::forward<Arg>(Args)...);
      }

      return;
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
    {
      if constexpr (Trivial)
      {
        getLargeRepr()->emplace(Key, std::forward<Arg>(Args)...);
      }
      else
      {
        if constexpr (StoreInPlace)
          getLargeRepr()->emplace(
            Key,
            std::optional<T>{std::in_place_t{}, std::forward<Arg>(Args)...});
        else
          getLargeRepr()->emplace(
            Key, std::make_unique<T>(std::forward<Arg>(Args)...));
      }

      ++Size;
      return;
    }

    if constexpr (Trivial)
      It->second = T{std::forward<Arg>(Args)...};
    else
    {
      if constexpr (StoreInPlace)
        It->second =
          std::optional<T>{std::in_place_t{}, std::forward<Arg>(Args)...};
      else
        It->second = std::make_unique<T>(std::forward<Arg>(Args)...);
    }
  }

  /// Deletes the element mapped to \p Key if such element exists
  ///
  /// \note This function potentially reallocates the storage buffer, and if
  /// \p StoreInPlace is \p true, invalidate \b EVERY reference and iterator
  /// to the remaining elements.
  void erase(KeyTy Key)
  {
    if (isSmall())
    {
      if (Key >= N)
        return;

      E& Elem = getSmallRepr()->at(Key);
      if (!isMapped(Elem))
        return;

      if constexpr (Trivial)
        Elem = T{};
      else
        Elem.reset();

      --Size;
      return;
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
      return;

    getLargeRepr()->erase(It);
    --Size;
    convertToSmallConditional();
  }

  /// Retrieve a non-mutable pointer to the element mapped for \p Key, or
  /// \p nullptr if \p Key is not mapped.
  const T* tryGet(KeyTy Key) const noexcept
  {
    if (isSmall())
    {
      if (Key >= N)
        return nullptr;

      const E& Elem = getSmallRepr()->at(Key);
      if (!isMapped(Elem))
        return nullptr;

      return &unwrap(Elem);
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
      return nullptr;
    return &unwrap(It->second);
  }
  /// Retrieve a mutable pointer to the element mapped for \p Key, or
  /// \p nullptr if \p Key is not mapped.
  NON_CONST_1_NOEXCEPT(T*, tryGet, KeyTy, Key);

  /// Retrieve a non-mutable reference to the element mapped for \p Key.
  /// \throws std::out_of_range if \p Key is not mapped.
  const T& get(KeyTy Key) const
  {
    const T* P = tryGet(Key);
    if (!P)
      out_of_range(Key);
    return *P;
  }
  /// Retrieve a mutable reference to the element mapped for \p Key.
  NON_CONST_1(T&, get, KeyTy, Key);

  /// Create a mutable reference to the element mapped for \p Key.
  /// If no such element exists, an appropriate default-constructed element is
  /// created and mapped for \p Key. The returned reference supports assigning
  /// value to it
  ///
  /// \note This function potentially reallocates the storage buffer, and if
  /// \p StoreInPlace is \p true, invalidate \b EVERY reference and iterator
  /// to existing elements.
  template <typename R = T>
  std::enable_if_t<UserTypeDefaultConstructible, R>& operator[](KeyTy Key)
  {
    if (isSmall())
    {
      if (Key >= N)
      {
        convertToLarge();
        return operator[](Key);
      }

      E& Elem = getSmallRepr()->at(Key);
      if (!isMapped(Elem))
      {
        Elem = constructValue();
        ++Size;
      }

      return unwrap(Elem);
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
    {
      It = getLargeRepr()->emplace(Key, constructValue()).first;
      ++Size;
    }
    return unwrap(It->second);
  }

private:
  // NOLINTNEXTLINE(readability-identifier-naming)
  [[noreturn]] void out_of_range(KeyTy Key) const
  {
    std::string Err = std::to_string(Key) + " is not mapped";
    throw std::out_of_range(Err);
  }

  /// \returns whether the stored element represents a mapped value.
  bool isMapped(const E& Elem) const noexcept
  {
    if constexpr (Trivial)
      return Elem != E{};
    else
      return static_cast<bool>(Elem);
  }

  template <typename R = E>
  std::enable_if_t<UserTypeDefaultConstructible, R> constructValue()
  {
    if constexpr (Trivial)
      return T{};
    else
    {
      if constexpr (StoreInPlace)
        return std::optional<T>(T{});
      else
        return std::make_unique<T>();
    }
  }

  const T& unwrap(const E& Elem) const
  {
    if constexpr (Trivial)
      return Elem;
    else
    {
      if (!isMapped(Elem))
        throw std::out_of_range("Element is not mapped!");
      return *Elem;
    }
  }
  NON_CONST_1(T&, unwrap, E&, Elem);

  const SmallRepresentation* getSmallRepr() const noexcept
  {
    return std::get_if<SmallRepresentation>(&Storage);
  }
  NON_CONST_0_NOEXCEPT(SmallRepresentation*, getSmallRepr);
  const LargeRepresentation* getLargeRepr() const noexcept
  {
    return std::get_if<LargeRepresentation>(&Storage);
  }
  NON_CONST_0_NOEXCEPT(LargeRepresentation*, getLargeRepr);

  /// Fills the small representation \p std::array with default initialised
  /// values.
  void fillSmallRepresentation()
  {
    if constexpr (Trivial || StoreInPlace)
      getSmallRepr()->fill(E{});
    else if constexpr (!StoreInPlace)
    {
      std::size_t M = N;
      while (M--)
        getSmallRepr()->at(M) = nullptr;
    }
  }

  /// Convers the representation to the small representation, if beneficial.
  /// This method does not always convert.
  void convertToSmallConditional()
  {
    if (isSmall())
      return;
    if (size() > MeaningfulSmallConversionThreshold)
      return;

    convertToSmall();
  }

  /// Converts the representation to the small representation, if possible.
  /// This method \b ALWAYS converts, if \p size() and the elements fit into
  /// \p N.
  void convertToSmall()
  {
    if (isSmall() || size() > N)
      return;
    // Check if the largest index fits the small buffer. If not, the rebalance
    // cannot happen.
    if (size() != 0 && (--getLargeRepr()->end())->first >= N)
      return;

    auto LR = std::get<LargeRepresentation>(std::move(Storage));
    auto& SR =
      Storage.template emplace<SmallRepresentation>(SmallRepresentation{});
    fillSmallRepresentation();

    for (auto It = LR.begin(); It != LR.end();)
    {
      SR.at(It->first) = std::move(It->second);
      It = LR.erase(It);
    }
  }

  /// Coverts the representation to the large representation, if currently in
  /// the small representation.
  ///
  /// \note This is a costly operation that rebalances the
  void convertToLarge()
  {
    if (isLarge())
      return;

    std::vector<std::size_t> IndicesToMove;
    {
      IndicesToMove.reserve(N);
      const SmallRepresentation* Data = getSmallRepr();
      for (std::size_t I = 0; I < N; ++I)
        if (isMapped(Data->at(I)))
          IndicesToMove.push_back(I);
    }

    auto SR = std::get<SmallRepresentation>(std::move(Storage));
    auto& LR =
      Storage.template emplace<LargeRepresentation>(LargeRepresentation{});

    for (std::size_t I : IndicesToMove)
      LR.emplace(I, std::move(SR.at(I)));
  }

#undef NON_CONST_1_NOEXCEPT
#undef NON_CONST_1
#undef NON_CONST_0_NOEXCEPT
#undef NON_CONST_0
};

} // namespace monomux
