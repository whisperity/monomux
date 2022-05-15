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
#include <type_traits>
#include <utility>

namespace monomux
{

namespace detail
{

/// Helper class for inheriting the dereference operators on \p UniqueScalar
/// if the entity inside it is a pointer.
template <class T, class Derived, bool IsPointer = true>
struct UniqueScalarPointerBase
{
  using Type = T;
  static_assert(std::is_pointer_v<Type>);
  using DerefType = std::remove_pointer_t<T>;
  using ConstDerefType = std::add_const_t<DerefType>;
  using ConstType = std::add_pointer_t<ConstDerefType>;

  DerefType& operator*() { return *get(); }
  ConstDerefType& operator*() const { return *get(); }

  Type operator->() noexcept { return get(); }
  ConstType operator->() const noexcept { return get(); }

private:
  Type get() noexcept { return static_cast<Derived*>(this)->get(); }
  ConstType get() const noexcept
  {
    return static_cast<const Derived*>(this)->get();
  }
};

/// Helper class for \e NOT inheriting the dereference operators if the entity
/// is not a pointer.
template <class T, class Derived>
struct UniqueScalarPointerBase<T, Derived, false>
{};

} // namespace detail

/// Wraps a movable scalar type which resets to the default value when
/// moved-from. Unlike \p unique_ptr, the value is allocated in-place.
template <typename T, T Default>
struct UniqueScalar
  : detail::
      UniqueScalarPointerBase<T, UniqueScalar<T, Default>, std::is_pointer_v<T>>
{
  /// Initialises the object with the default value.
  UniqueScalar() noexcept(std::is_nothrow_default_constructible_v<T>)
    : Value(Default)
  {
    static_assert(sizeof(*this) == sizeof(T), "Extra padding is forbidden!");
  }
  /// Initialises the object with the specified value
  UniqueScalar(T Value) noexcept(std::is_nothrow_copy_constructible_v<T>)
    : Value(Value)
  {}
  /// Move-initialises the value from \p RHS, and resets \p RHS to the
  /// \p Default.
  UniqueScalar(UniqueScalar&& RHS) noexcept(
    std::is_nothrow_move_constructible_v<T>&&
      std::is_nothrow_move_assignable_v<T>)
    : Value(std::move(RHS.Value))
  {
    RHS.Value = Default;
  }
  /// Move-assigns the value from \p RHS, and resets \p RHS to the \p Default.
  UniqueScalar&
  operator=(UniqueScalar&& RHS) noexcept(std::is_nothrow_move_assignable_v<T>)
  {
    if (this == &RHS)
      return *this;

    Value = std::move(RHS.Value);
    RHS.Value = Default;
    return *this;
  }
  ~UniqueScalar() noexcept = default;

  /// Converts to the stored value.
  operator T&() noexcept { return Value; }
  /// Converts to the stored value.
  operator const T&() const noexcept { return Value; }

  /// Converts to the stored value.
  T& get() noexcept { return Value; }
  /// Converts to the stored value.
  const T& get() const noexcept { return Value; }

  /// Sets \p NewValue into the wrapped object.
  UniqueScalar&
  operator=(T&& NewValue) noexcept(std::is_nothrow_move_assignable_v<T>)
  {
    Value = std::forward<T>(NewValue);
    return *this;
  }

private:
  T Value;
};

} // namespace monomux
