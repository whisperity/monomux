/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace monomux
{

/// Implements an iterator \e adaptor that adapts an iterator to be able to
/// continue iterating past its "local" end by leaping into a new "bucket".
template <typename Derived, typename InnerIterator>
class forward_bucket_iterator_adaptor
{
public:
  using iterator_type = InnerIterator;
  using iterator_category = std::forward_iterator_tag;
  using difference_type = typename iterator_type::difference_type;
  using value_type = typename iterator_type::value_type;
  using pointer = typename iterator_type::pointer;
  using reference = typename iterator_type::reference;

protected:
  iterator_type It;
  iterator_type LocalEnd;

public:
  forward_bucket_iterator_adaptor() noexcept(
    std::is_nothrow_default_constructible_v<iterator_type>)
    : It(), LocalEnd()
  {}
  forward_bucket_iterator_adaptor(
    iterator_type Iterator,
    iterator_type
      BucketLocalEnd) noexcept(std::is_nothrow_constructible_v<iterator_type,
                                                               iterator_type>)
    : It(Iterator), LocalEnd(BucketLocalEnd)
  {}

  [[nodiscard]] bool
  operator==(const forward_bucket_iterator_adaptor& RHS) const
    noexcept(noexcept(std::declval<const iterator_type>() ==
                      std::declval<const iterator_type>()))
  {
    return It == RHS.It && LocalEnd == RHS.LocalEnd;
  }
  [[nodiscard]] bool
  operator!=(const forward_bucket_iterator_adaptor& RHS) const noexcept(
    noexcept(std::declval<const forward_bucket_iterator_adaptor>().operator==(
      std::declval<const forward_bucket_iterator_adaptor>())))
  {
    return !(*this == RHS);
  }

  forward_bucket_iterator_adaptor&
  operator++() noexcept(noexcept(std::declval<iterator_type>().operator++()))
  {
    ++It;
    if (It == LocalEnd)
    {
      // If the stepping of the iterator reaches the local end of the bucket,
      // ask the real iterator we're helping to adapt to give us a potential
      // new range from the next bucket.
      Derived::set_next_bucket(static_cast<Derived*>(this));
    }
    return *this;
  }
  [[nodiscard]] forward_bucket_iterator_adaptor operator++(int) noexcept(
    noexcept(std::declval<iterator_type>().operator++(1)))
  {
    auto Tmp = *this;
    ++(*this);
    return Tmp;
  }

  [[nodiscard]] reference operator*() const
    noexcept(noexcept(std::declval<const iterator_type>().operator*()))
  {
    return It.operator*();
  }
  [[nodiscard]] pointer operator->() noexcept(
    noexcept(std::declval<const iterator_type>().operator->()))
  {
    return It.operator->();
  }
};

} // namespace monomux
