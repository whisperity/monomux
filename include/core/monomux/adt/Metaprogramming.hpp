/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>

/* NOLINTBEGIN(readability-identifier-naming): Matching STL code... */

/// Implements helper function and class templates, \p constexpr functions and
/// similar solutions to aid with template metaprogramming, mostly by the
/// manipulation of typelists: compile-time variadic-length containers of
/// different types, mostly different instantiations of the same record
/// template.
namespace monomux::meta
{

using index_t = std::size_t;
struct invalid_index_t
{};

// (Wow, this is only available starting C++20...)
template <typename T> struct type_identity
{
  using type = T;
};
template <typename T> using type_identity_t = typename type_identity<T>::type;

template <typename T, typename U> struct pair
{
  using first = T;
  using second = U;
};

namespace detail
{

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
to_array_impl(T (&A)[N], std::index_sequence<I...> /*Seq*/)
{
  return {{A[I]...}};
}

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N>
to_array_impl(T (&&A)[N], std::index_sequence<I...> /*Seq*/)
{
  return {{std::move(A[I])...}};
}

} // namespace detail

// Available officially starting C++20...
template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&A)[N])
{
  return detail::to_array_impl(A, std::make_index_sequence<N>{});
}

template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&A)[N])
{
  return detail::to_array_impl(std::move(A), std::make_index_sequence<N>{});
}

// (Not available in C++20.)
/// Converts an \p std::integer_sequence to a \p std::array.
template <class T, T... Ts>
constexpr std::array<std::remove_cv_t<T>, sizeof...(Ts)>
to_array(std::integer_sequence<T, Ts...> /*Seq*/)
{
  return std::array<T, sizeof...(Ts)>{Ts...};
}

template <typename Index, typename T> struct find_result_t
{
  using index = Index;
  using type = T;
  static inline constexpr const bool value = true;
};
template <typename T> struct find_result_t<invalid_index_t, T>
{
  using index = invalid_index_t;
  static inline constexpr const bool value = false;
};
using not_found_result_t = find_result_t<invalid_index_t, invalid_index_t>;

namespace compare
{

template <class L, class R> struct less
{
  static inline constexpr const bool value = L::value < R::value;
  using type = std::conditional_t<value, L, R>;
};

template <class L, class R> struct equal
{
  static inline constexpr const bool value = L::value == R::value;
};

template <class L, class R> struct greater
{
  static inline constexpr const bool value = L::value > R::value;
  using type = std::conditional_t<value, L, R>;
};

} // namespace compare

namespace List
{

/// Unhandled/empty list.
template <class...> struct L
{
  static constexpr const std::size_t Size = 0;
};
using EmptyList = L<>;
/// Shorthand syntax for an empty list.
using empty = EmptyList;
/// Single element list.
template <class Elem> struct L<Elem>
{
  using Head = Elem;
  using Tail = EmptyList;

  static inline constexpr const std::size_t Size = 1;
};
/// Multi-element list implemented with Head-Tail semantics.
template <class H, class... Ts> struct L<H, Ts...>
{
  using Head = H;
  using Tail = L<Ts...>;

  static inline constexpr const std::size_t Size = 1 + Tail::Size;
};

template <class... Es> struct Size
{
  static inline constexpr const auto value = sizeof...(Es);
};
template <class... Es> struct Size<L<Es...>>
{
  static inline constexpr const auto value = L<Es...>::Size;
};

/// Get the \p Ith element of the list.
template <index_t I, class H, class... Ts> struct Access;
template <index_t I, class H, class... Ts> struct Access<I, L<H, Ts...>>
{
  using type = typename Access<(I - 1), L<Ts...>>::type;
};
template <class H, class... Ts> struct Access<1, L<H, Ts...>>
{
  using type = H;
};

/// Get the \p Ith element of the list, or a \p Default if it doesn't exist.
template <index_t I, class Default, class List> struct AccessOrDefault;
template <index_t I, class Default>
struct AccessOrDefault<I, Default, EmptyList>
{
  using type = Default;
};
template <index_t I, class Default, class H, class... Ts>
struct AccessOrDefault<I, Default, L<H, Ts...>>
{
private:
  static inline constexpr auto Impl()
  {
    constexpr const auto S = Size<L<H, Ts...>>::value;
    if constexpr (I > S)
      return type_identity<Default>{};
    else
      return type_identity<typename Access<I, L<H, Ts...>>::type>{};
  }

public:
  using type = typename decltype(Impl())::type;
};

/// Add an element to the end of a list.
template <class E, class... Es> struct Append;
template <class E> struct Append<E, EmptyList>
{
  using type = L<E>;
};
template <class E, class... Es> struct Append<E, L<Es...>>
{
  using type = L<Es..., E>;
};

/// Add an element to the beginning of a list.
template <class E, class... Es> struct Prepend;
template <class E> struct Prepend<E, EmptyList>
{
  using type = L<E>;
};
template <class E, class... Es> struct Prepend<E, L<Es...>>
{
  using type = L<E, Es...>;
};

/// Add all elements of list \p L2 at the end of list \p L1.
template <class L1, class L2> struct Concat;
template <> struct Concat<EmptyList, EmptyList>
{
  using type = EmptyList;
};
template <class... Ts> struct Concat<EmptyList, L<Ts...>>
{
  using type = L<Ts...>;
};
template <class... Ts>
struct Concat<L<Ts...>, EmptyList> : public Concat<EmptyList, L<Ts...>>
{};
template <class... Ts, class... Us> struct Concat<L<Ts...>, L<Us...>>
{
  using type = L<Ts..., Us...>;
};

template <class L> struct Reverse;
template <> struct Reverse<EmptyList>
{
  using type = EmptyList;
};
template <class E> struct Reverse<L<E>>
{
  using type = L<E>;
};
template <class H, class... Ts> struct Reverse<L<H, Ts...>>
{
  using type = typename Append<H, typename Reverse<L<Ts...>>::type>::type;
};

template <index_t I, index_t N, class L> struct Substr;
template <index_t I, index_t N> struct Substr<I, N, EmptyList>
{
  using type = EmptyList;
};
template <index_t I, index_t N, class H, class... Ts>
struct Substr<I, N, L<H, Ts...>>
{
private:
  static inline constexpr auto Impl()
  {
    if constexpr (N == 0)
      return type_identity<EmptyList>{};
    else if constexpr (I > 1)
      return type_identity<typename Substr<(I - 1), N, L<Ts...>>::type>{};
    else
    {
      if constexpr (N == 1)
        return type_identity<L<H>>{};
      else
      {
        using Cont = typename Substr<1, (N - 1), L<Ts...>>::type;
        return type_identity<typename Prepend<H, Cont>::type>{};
      }
    }
  }

public:
  using type = typename decltype(Impl())::type;
};

template <index_t I, class L> struct Split;
template <index_t I> struct Split<I, EmptyList>
{
  using type = pair<EmptyList, EmptyList>;
};
template <index_t I, class... Es> struct Split<I, L<Es...>>
{
private:
  using left = typename Substr<1, (I - 1), L<Es...>>::type;
  using right = typename Substr<I, Size<L<Es...>>::value, L<Es...>>::type;

public:
  using type = pair<left, right>;
};

template <index_t I, class E, class L> struct Replace;
template <index_t I, class E> struct Replace<I, E, EmptyList>;
template <class E, class H> struct Replace<0, E, L<H>>
{
  using type = L<E>;
};
template <index_t I, class E, class H, class... Ts>
struct Replace<I, E, L<H, Ts...>>
{
private:
  using pair = typename Split<I, L<H, Ts...>>::type;
  using prefix = typename pair::first;
  using suffix = typename pair::second::Tail;

public:
  using type = typename Concat<typename Append<E, prefix>::type, suffix>::type;
};

template <index_t Current, class E, class L> struct IndexOf;
template <index_t Current, class E> struct IndexOf<Current, E, EmptyList>
{
  using type = invalid_index_t;
};
template <index_t Current, class E, class H> struct IndexOf<Current, E, L<H>>
{
  using type = std::conditional_t<std::is_same_v<E, H>,
                                  std::integral_constant<index_t, Current>,
                                  invalid_index_t>;
};
template <index_t Current, class E, class H, class... Ts>
struct IndexOf<Current, E, L<H, Ts...>>
{
  using type =
    std::conditional_t<std::is_same_v<E, H>,
                       std::integral_constant<index_t, Current>,
                       typename IndexOf<Current + 1, E, L<Ts...>>::type>;
};

template <template <class E> class P, class L> struct Filter;
template <template <class E> class P> struct Filter<P, EmptyList>
{
  using type = EmptyList;
};
template <template <class E> class P, class H> struct Filter<P, L<H>>
{
  using type = typename std::conditional_t<P<H>::value, L<H>, EmptyList>;
};
template <template <class E> class P, class H, class... Ts>
struct Filter<P, L<H, Ts...>>
{
  using type = typename Concat<typename Filter<P, L<H>>::type,
                               typename Filter<P, L<Ts...>>::type>::type;
};

template <index_t Current, template <class E> class P, class L> struct Find;
template <index_t Current, template <class E> class P>
struct Find<Current, P, EmptyList>
{
  using type = not_found_result_t;
};
template <index_t Current, template <class E> class P, class H>
struct Find<Current, P, L<H>>
{
  using type = std::conditional_t<
    P<H>::value,
    find_result_t<std::integral_constant<index_t, Current>, H>,
    not_found_result_t>;
};
template <index_t Current, template <class E> class P, class H, class... Ts>
struct Find<Current, P, L<H, Ts...>>
{
  using type = std::conditional_t<
    P<H>::value,
    find_result_t<std::integral_constant<index_t, Current>, H>,
    typename Find<Current + 1, P, L<Ts...>>::type>;
};

template <template <class E> class Fn, class L> struct Map;
template <template <class E> class Fn> struct Map<Fn, EmptyList>
{
  using type = EmptyList;
};
template <template <class E> class Fn, class H, class... Ts>
struct Map<Fn, L<H, Ts...>>
{
  using type = typename Prepend<typename Fn<H>::type,
                                typename Map<Fn, L<Ts...>>::type>::type;
};

/// Applies a variadic metafunction \p Fn on the elements of the list \p L
/// after unpacking it.
template <template <class...> class Fn, class L> struct Apply;
template <template <class...> class Fn> struct Apply<Fn, EmptyList>
{
  using type = typename Fn<>::type;
};
template <template <class...> class Fn, class H, class... Ts>
struct Apply<Fn, L<H, Ts...>>
{
  using type = typename Fn<H, Ts...>::type;
};

template <template <class L, class R> class Comp, class List> struct Min;
template <template <class LHS, class RHS> class Comp, class H>
struct Min<Comp, L<H>>
{
  using type = H;
  static inline constexpr const auto value = type::value;
};
template <template <class LHS, class RHS> class Comp, class H, class... Ts>
struct Min<Comp, L<H, Ts...>>
{
private:
  static inline constexpr auto Impl()
  {
    using TailMin = typename Min<Comp, L<Ts...>>::type;
    if constexpr (Comp<H, TailMin>::value)
      return type_identity<H>{};
    else
      return type_identity<typename TailMin::type>{};
  }

public:
  using type = typename decltype(Impl())::type;
  static inline constexpr auto value = type::value;
};

template <template <class L, class R> class Comp, class List> struct Max;
template <template <class LHS, class RHS> class Comp, class H>
struct Max<Comp, L<H>>
{
  using type = H;
  static inline constexpr const auto value = type::value;
};
template <template <class LHS, class RHS> class Comp, class H, class... Ts>
struct Max<Comp, L<H, Ts...>>
{
private:
  static inline constexpr auto Impl()
  {
    using TailMax = typename Max<Comp, L<Ts...>>::type;
    if constexpr (!Comp<H, TailMax>::value)
      return type_identity<H>{};
    else
      return type_identity<typename TailMax::type>{};
  }

public:
  using type = typename decltype(Impl())::type;
  static inline constexpr const auto value = type::value;
};

} // namespace List

/// Create a type list from the given elements.
template <class... Es> using list = List::L<Es...>;

/// An empty list.
using empty_list = List::empty;

/// The head element of a (non-empty) list.
template <class L> using head_t = typename L::Head;

/// The tail of the list. It might be empty.
template <class L> using tail_t = typename L::Tail;

/// Returns the size of a parameter pack, or a list.
template <class... Es>
static inline constexpr const auto size_v = List::Size<Es...>::value;

/// Retrieve the element at index \p I from the \p L list.
///
/// \note Lists are indexed starting at \b 1.
template <index_t I, class L>
using access_t = typename List::Access<I, L>::type;

/// Retrieve the element at index \p I from the \p L list, or \p Default, if
/// the requested element does not exist.
///
/// \note Lists are indexed starting at \b 1.
template <index_t I, class Default, class L>
using access_or_default_t = typename List::AccessOrDefault<I, Default, L>::type;

/// Appends the element \p E to the end of the list containing the \p Es
/// elements.
template <class E, class... Es>
using append_t = typename List::Append<E, Es...>::type;

/// Appends the element \p E to the beginning of the list containing the \p Es
/// elements.
template <class E, class... Es>
using prepend_t = typename List::Prepend<E, Es...>::type;

/// Concatenates the list \p L2 after the end of the list \p L1.
template <class L1, class L2>
using concat_t = typename List::Concat<L1, L2>::type;

/// Reverses the list \p L.
template <class L> using reverse_t = typename List::Reverse<L>::type;

/// Takes the range from index \p I having length \p N, i.e., "[I, I + N]" from
/// the list \p L.
template <class L, index_t I, index_t N>
using substr_t = typename List::Substr<I, N, L>::type;

/// Splits the list \p L into two lists at the index \p I. The element at
/// \p I will be added to the \b second list.
template <class L, index_t I> using split_t = typename List::Split<I, L>::type;

/// Replaces the element at index \p I with \p E in the list \p L.
/// If index \p I would overflow the length of \p L, the replacement is
/// undefined.
template <class L, index_t I, class E>
using replace_t = typename List::Replace<I, E, L>::type;

/// Returns the \p std::integral_constant index of the \e first element in
/// \p L that is equal to (when considering \p std::is_same) the parameter
/// \p E if such elements exist, or \p invalid_index_t otherwise.
template <class L, class E>
using maybe_index_t = typename List::IndexOf<1, E, L>::type;

/// Returns the index of the \e first element in \p L that is equal to
/// (when considering \p std::is_same) the parameter \p E. Otherwise, the
/// inner member constant \p value is not present and thus \p index_v does not
/// compile.
template <class L, class E>
static constexpr const index_t index_v = maybe_index_t<L, E>::value;

/// Creates a list of the elements of \p L that match the \p Predicate.
/// (The \p value member of \p Predicate<Element> must be \p true).
template <template <class> class Predicate, class L>
using filter_t = typename List::Filter<Predicate, L>::type;

/// Searches for the first element in the \p L list that matches the
/// \p Predicate, returning both the found item and its index.
/// If no results are found, \p invalid_index_t is returned.
/// (The \p value member of \p Predicate<Element> must be \p true to match.)
template <template <class> class Predicate, class L>
using find_t = typename List::Find<1, Predicate, L>::type;

/// Creates a list of the elements of \p L by individually transforming each
/// using the \p Fn (meta)function.
/// (The \p type member-alias of \p Fn<Element> must be the result of the
/// transformation).
template <template <class> class Fn, class L>
using map_t = typename List::Map<Fn, L>::type;

/// Returns whether all elements of \p L satisfy the \p Predicate.
template <template <class> class Predicate, class L>
static inline constexpr const bool all_v =
  List::Apply<std::conjunction, map_t<Predicate, L>>::type::value;
/// Returns whether any element of \p L satisfies the \p Predicate.
template <template <class> class Predicate, class L>
static inline constexpr const bool any_v =
  List::Apply<std::disjunction, map_t<Predicate, L>>::type::value;
/// Returns whether none of the elements of \p L satisfy the \p Predicate.
template <template <class> class Predicate, class L>
static inline constexpr const bool none_v = !all_v<Predicate, L>;

/// Returns the minimum element of the \p L list, based on the \p Comp
/// comparator.
template <class L, template <class, class> class Comp = compare::less>
using min_element_t = typename List::Min<Comp, L>::type;
/// Returns the \p value of the minimum element of the \p L list, based on the
/// \p Comp comparator.
template <class L, template <class, class> class Comp = compare::less>
static inline constexpr const auto min_v = min_element_t<L, Comp>::value;

/// Returns the maximum element of the \p L list, based on the \p Comp
/// comparator.
template <class L, template <class, class> class Comp = compare::less>
using max_element_t = typename List::Max<Comp, L>::type;
/// Returns the \p value of the maximum element of the \p L list, based on the
/// \p Comp comparator.
template <class L, template <class, class> class Comp = compare::less>
static inline constexpr const auto max_v = max_element_t<L, Comp>::value;

namespace detail
{

template <typename T, T... Ts>
inline constexpr auto
make_integral_constants(std::integer_sequence<T, Ts...> /*Seq*/)
{
  return List::L<std::integral_constant<T, Ts>...>{};
}

template <template <class...> class List, class... Es>
constexpr std::array<std::remove_cv_t<decltype(List<Es...>::Head::value)>,
                     size_v<List<Es...>>>
integral_constants_to_array_impl(List<Es...>&& /*L*/)
{
  return std::array{Es::value...};
}

} // namespace detail

/// Converts an \p integer_sequence into a lits of \p std::integral_constants.
template <class IntegerSequence>
using make_integral_constants_t =
  decltype(detail::make_integral_constants(IntegerSequence{}));

/// Converts an \p integral_constants_t to a \p std::array.
template <class L>
constexpr std::array<std::remove_cv_t<decltype(L::Head::value)>, size_v<L>>
integral_constants_to_array()
{
  return detail::integral_constants_to_array_impl(L{});
}

} // namespace monomux::meta

/* NOLINTEND(readability-identifier-naming) */
