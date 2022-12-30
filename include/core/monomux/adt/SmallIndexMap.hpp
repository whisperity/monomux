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

#include "monomux/adt/FunctionExtras.hpp"

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
/// \tparam IntrusiveDefaultSentinel Whether to consider the default-constructed
/// value to be the "unmapped" value. If \p true, \p T must be default
/// constructible. Emplacing or setting the default value manually is undefined.
template <typename T,
          std::size_t N,
          bool StoreInPlace = true,
          bool IntrusiveDefaultSentinel = std::is_pointer_v<T>,
          typename KeyTy = std::size_t>
class SmallIndexMap
{
  /// The threshold at which point the small representation will be re-engaged.
  static constexpr std::size_t MeaningfulSmallConversionThreshold = N / 2;

  static_assert(std::is_integral_v<KeyTy> && std::is_unsigned_v<KeyTy>,
                "Keys must be index-like for small representation to work!");

  /// Whether the user's value type \p T should be wrapped in a "nullable"
  /// object, or can be stored verbatim.
  static constexpr bool NeedsMaybe = !IntrusiveDefaultSentinel;

  /// The actually stored element type inside the containers.
  using E = std::conditional_t<
    !NeedsMaybe,
    T,
    std::conditional_t<StoreInPlace, std::optional<T>, std::unique_ptr<T>>>;

  static_assert(
    !IntrusiveDefaultSentinel || std::is_default_constructible_v<T>,
    "IntrusiveDefaultSentinel requires T to be default constructible!");

  static_assert(!IntrusiveDefaultSentinel || StoreInPlace,
                "If the stored value's default is the sentinel marking, it is "
                "automatically stored in place, and specifying StoreInPlace = "
                "false is useless.");

  static_assert(std::is_default_constructible_v<E> &&
                  std::is_move_assignable_v<E>,
                "The detail storage element type must be default constructible "
                "and moveable.");

  using SmallRepresentation = std::array<E, N>;
  using LargeRepresentation = std::map<KeyTy, E>;
  std::variant<SmallRepresentation, LargeRepresentation> Storage;

  /// The number of mapped elements.
  std::size_t Size = 0;

public:
  /// Initialises an empty \p SmallIndexMap that starts in the small
  /// representation.
  SmallIndexMap() : Storage(SmallRepresentation{})
  {
    fillSmallRepresentation();
  }

  /// \returns Whether the data structure is currently in the small
  /// representation. In this mode, access of data is a constant operation.
  [[nodiscard]] bool isSmall() const noexcept
  {
    return std::holds_alternative<SmallRepresentation>(Storage);
  }
  /// \returns Whether the data structure is currently in the large
  /// representation. In this mode, access of data is a logarithmic operation.
  [[nodiscard]] bool isLarge() const noexcept { return !isSmall(); }

  /// \returns the size of the container, i.e. the number of elements added
  /// into it.
  ///
  /// This is a \e constant-time query.
  [[nodiscard]] std::size_t size() const noexcept { return Size; }

  /// \returns whether the container is empty.
  ///
  /// This is a \e constant-time query.
  [[nodiscard]] bool empty() const noexcept { return Size == 0; }

  /// \returns whether the \p Key is mapped.
  [[nodiscard]] bool contains(KeyTy Key) const noexcept
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
  /// If \p T is default constructible and \p IntrusiveDefaultSentinel is
  /// \p true, care must be taken to emplace a \b non-default value into the
  /// object constructed by this function. Otherwise, the \p size() of the
  /// object will not be properly calculated.
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

      if constexpr (!NeedsMaybe)
        Elem = T{std::forward<Arg>(Args)...};
      else
      {
        if constexpr (StoreInPlace)
          Elem = std::optional<T>{std::in_place, std::forward<Arg>(Args)...};
        else
          Elem = std::make_unique<T>(std::forward<Arg>(Args)...);
      }

      return;
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
    {
      if constexpr (!NeedsMaybe)
      {
        getLargeRepr()->emplace(Key, std::forward<Arg>(Args)...);
      }
      else
      {
        if constexpr (StoreInPlace)
          getLargeRepr()->emplace(
            Key, std::optional<T>{std::in_place, std::forward<Arg>(Args)...});
        else
          getLargeRepr()->emplace(
            Key, std::make_unique<T>(std::forward<Arg>(Args)...));
      }

      ++Size;
      return;
    }

    if constexpr (IntrusiveDefaultSentinel)
      It->second = T{std::forward<Arg>(Args)...};
    else
    {
      if constexpr (StoreInPlace)
        It->second =
          std::optional<T>{std::in_place, std::forward<Arg>(Args)...};
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

      if constexpr (IntrusiveDefaultSentinel)
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

  /// Deletes all mapped elements.
  void clear()
  {
    if (isSmall())
    {
      for (KeyTy K = 0; K < N; ++K)
      {
        E& Elem = getSmallRepr()->at(K);
        if constexpr (IntrusiveDefaultSentinel)
          Elem = T{};
        else
          Elem.reset();
      }

      Size = 0;
      return;
    }

    getLargeRepr()->clear();
    Size = 0;
    convertToSmall();
  }

  /// Retrieve a non-mutable pointer to the element mapped for \p Key, or
  /// \p nullptr if \p Key is not mapped.
  [[nodiscard]] const T* tryGet(KeyTy Key) const noexcept
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
  MONOMUX_MEMBER_1(T*, tryGet, [[nodiscard]], noexcept, KeyTy, Key);

  /// Retrieve a non-mutable reference to the element mapped for \p Key.
  /// \throws std::out_of_range if \p Key is not mapped.
  [[nodiscard]] const T& get(KeyTy Key) const
  {
    const T* P = tryGet(Key);
    if (!P)
      out_of_range(Key);
    return *P;
  }
  /// Retrieve a mutable reference to the element mapped for \p Key.
  MONOMUX_MEMBER_1(T&, get, [[nodiscard]], , KeyTy, Key);

  /// Create a mutable reference to the element mapped for \p Key.
  /// If no such element exists, an appropriate default-constructed element is
  /// created and mapped for \p Key. The returned reference supports assigning
  /// value to it
  ///
  /// If \p T is default constructible and \p IntrusiveDefaultSentinel is
  /// \p true, care must be taken to assign a \b non-default value to the
  /// object constructed by this operator. Otherwise, the \p size() of the
  /// object will not be properly calculated.
  ///
  /// \note This function potentially reallocates the storage buffer, and if
  /// \p StoreInPlace is \p true, invalidate \b EVERY reference and iterator
  /// to existing elements.
  template <typename R = T>
  [[nodiscard]] std::enable_if_t<std::is_default_constructible_v<T>, R>&
  operator[](KeyTy Key)
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
        Elem = constructElement();
        ++Size;
      }

      return unwrap(Elem);
    }

    auto It = getLargeRepr()->find(Key);
    if (It == getLargeRepr()->end())
    {
      It = getLargeRepr()->emplace(Key, constructElement()).first;
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
  [[nodiscard]] bool isMapped(const E& Elem) const noexcept
  {
    if constexpr (IntrusiveDefaultSentinel)
      return Elem != E{};
    else
      return static_cast<bool>(Elem);
  }

  template <typename R = E>
  [[nodiscard]] std::enable_if_t<std::is_default_constructible_v<T>, R>
  constructElement()
  {
    if constexpr (!NeedsMaybe)
      return T{};
    else
    {
      if constexpr (StoreInPlace)
        return std::optional<T>(std::in_place, T{});
      else
        return std::make_unique<T>();
    }
  }

  [[nodiscard]] const T& unwrap(const E& Elem) const
  {
    if constexpr (!NeedsMaybe)
      return Elem;
    else
    {
      if (!isMapped(Elem))
        throw std::out_of_range("Element is not mapped!");
      return *Elem;
    }
  }
  MONOMUX_MEMBER_1(T&, unwrap, [[nodiscard]], , E&, Elem);

  [[nodiscard]] const SmallRepresentation* getSmallRepr() const noexcept
  {
    return std::get_if<SmallRepresentation>(&Storage);
  }
  MONOMUX_MEMBER_0(SmallRepresentation*, getSmallRepr, [[nodiscard]], noexcept);
  [[nodiscard]] const LargeRepresentation* getLargeRepr() const noexcept
  {
    return std::get_if<LargeRepresentation>(&Storage);
  }
  MONOMUX_MEMBER_0(LargeRepresentation*, getLargeRepr, [[nodiscard]], noexcept);

  /// Fills the small representation \p std::array with default initialised
  /// values.
  void fillSmallRepresentation()
  {
    if constexpr (!NeedsMaybe || StoreInPlace)
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
  /// \note This is a costly operation that rebalances the lookup map.
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
};

} // namespace monomux
