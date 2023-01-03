/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <atomic>
#include <type_traits>
#include <utility>

namespace monomux
{

/// Wrapper class over \p std::atomic<T> that enables "copying" and "moving" the
/// value contained by non-atomically initialising a \b new \p atomic with the
/// current contained value.
template <typename T> class Atomic
{
  using AT = std::atomic<T>;
  static constexpr bool IsNoExcept = noexcept(
    std::declval<AT>().store(T{}))&& noexcept(std::declval<AT>().load());

  AT Value = ATOMIC_VAR_INIT(T{});

public:
  Atomic() noexcept(std::is_nothrow_default_constructible_v<AT>) = default;
  Atomic(T&& Val) noexcept(IsNoExcept) { Value.store(std::forward<T>(Val)); }
  Atomic(const Atomic& RHS) noexcept(IsNoExcept)
  {
    Value.store(RHS.Value.load());
  }
  Atomic(Atomic&& RHS) noexcept(IsNoExcept) { Value.store(RHS.Value.load()); }
  Atomic& operator=(const Atomic& RHS) noexcept(IsNoExcept)
  {
    if (this == &RHS)
      return *this;

    Value.store(RHS.Value.load());
    return *this;
  }
  Atomic& operator=(Atomic&& RHS) noexcept(IsNoExcept)
  {
    if (this == &RHS)
      return *this;

    Value.store(RHS.Value.load());
    return *this;
  }
  ~Atomic() = default;

  [[nodiscard]] std::atomic<T>& get() noexcept { return Value; }
  [[nodiscard]] const std::atomic<T>& get() const noexcept { return Value; }

  [[nodiscard]] T load() const noexcept(IsNoExcept) { return Value.load(); }
  void store(const T& NewValue) noexcept(IsNoExcept)
  {
    return Value.store(NewValue);
  }
  void store(T&& NewValue) noexcept(IsNoExcept)
  {
    return Value.store(std::move(NewValue));
  }
};

} // namespace monomux
