/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>

#include "monomux/adt/Metaprogramming.hpp"

/* NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */

TEST(Metaprogramming, Main)
{
  // This is not a real test, as the metaprogramming library is verified with
  // static_assert()s above.
  ;
}

/* NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-goto,cppcoreguidelines-owning-memory)
 */

using namespace monomux::meta;

static_assert(std::is_same_v<head_t<list<int, double>>, int>);
static_assert(std::is_same_v<tail_t<list<int, double>>, list<double>>);
static_assert(std::is_same_v<tail_t<list<int>>, empty_list>);

static_assert(size_v<empty_list> == 0);
static_assert(size_v<int, void> == 2);
static_assert(size_v<int, void> == size_v<list<int, void>>);

static_assert(std::is_same_v<int, list<int>::Head>);
static_assert(std::is_same_v<double, list<int, double>::Tail::Head>);
static_assert(std::is_same_v<int, access_t<1, list<int>>>);
static_assert(std::is_same_v<int, access_t<1, list<int, double>>>);
static_assert(std::is_same_v<double, access_t<2, list<int, double>>>);

static_assert(
  std::is_same_v<double, access_or_default_t<2, char, list<int, double>>>);
static_assert(std::is_same_v<char, access_or_default_t<2, char, list<int>>>);

static_assert(std::is_same_v<list<int>, append_t<int, empty_list>>);
static_assert(std::is_same_v<list<int, double>, append_t<double, list<int>>>);
static_assert(std::is_same_v<list<int, double, void>,
                             append_t<void, append_t<double, list<int>>>>);

static_assert(std::is_same_v<list<int>, prepend_t<int, empty_list>>);
static_assert(std::is_same_v<list<double, int>, prepend_t<double, list<int>>>);
static_assert(std::is_same_v<list<void, double, int>,
                             prepend_t<void, prepend_t<double, list<int>>>>);

static_assert(
  std::is_same_v<list<int, double>, concat_t<list<int>, list<double>>>);
static_assert(std::is_same_v<list<int, void, double, void>,
                             concat_t<list<int, void>, list<double, void>>>);
static_assert(std::is_same_v<list<int>, concat_t<empty_list, list<int>>>);
static_assert(std::is_same_v<list<int>, concat_t<list<int>, empty_list>>);

static_assert(std::is_same_v<empty_list, reverse_t<empty_list>>);
static_assert(std::is_same_v<list<int>, reverse_t<list<int>>>);
static_assert(std::is_same_v<list<double, int>, reverse_t<list<int, double>>>);
static_assert(
  std::is_same_v<list<void, double, int>, reverse_t<list<int, double, void>>>);

static_assert(std::is_same_v<empty_list, substr_t<empty_list, 1, 2>>);
static_assert(std::is_same_v<list<int>, substr_t<list<int>, 1, 1>>);
static_assert(
  std::is_same_v<list<char>, substr_t<list<char, short, int, long>, 1, 1>>);
static_assert(std::is_same_v<list<char, short>,
                             substr_t<list<char, short, int, long>, 1, 2>>);
static_assert(std::is_same_v<list<char, short, int>,
                             substr_t<list<char, short, int, long>, 1, 3>>);
static_assert(std::is_same_v<list<short, int>,
                             substr_t<list<char, short, int, long>, 2, 2>>);
static_assert(
  std::is_same_v<list<int>, substr_t<list<char, short, int, long>, 3, 1>>);

static_assert(std::is_same_v<pair<empty_list, list<char, short, int, long>>,
                             split_t<list<char, short, int, long>, 1>>);
static_assert(std::is_same_v<pair<list<char>, list<short, int, long>>,
                             split_t<list<char, short, int, long>, 2>>);
static_assert(std::is_same_v<pair<list<char, short>, list<int, long>>,
                             split_t<list<char, short, int, long>, 3>>);
static_assert(std::is_same_v<pair<list<char, short, int>, list<long>>,
                             split_t<list<char, short, int, long>, 4>>);

static_assert(std::is_same_v<replace_t<list<double>, 1, void>, list<void>>);
static_assert(
  std::is_same_v<replace_t<list<double, long>, 1, void>, list<void, long>>);
static_assert(
  std::is_same_v<replace_t<list<double, long>, 2, void>, list<double, void>>);

static_assert(index_v<list<char, short, int>, char> == 1);
static_assert(index_v<list<char, short, int>, short> == 2);
static_assert(index_v<list<char, short, int>, int> == 3);

static_assert(std::is_same_v<maybe_index_t<list<char, short, int>, int>,
                             std::integral_constant<index_t, 3>>);
static_assert(std::is_same_v<maybe_index_t<list<char, short, int>, double>,
                             invalid_index_t>);

static_assert(
  std::is_same_v<find_t<std::is_void, empty_list>, not_found_result_t>);
static_assert(
  std::is_same_v<find_t<std::is_void, list<int, double>>, not_found_result_t>);
static_assert(
  std::is_same_v<find_t<std::is_void, list<void, int>>::type, void>);
static_assert(std::is_same_v<find_t<std::is_void, list<void, int>>::index,
                             std::integral_constant<index_t, 1>>);
static_assert(
  std::is_same_v<find_t<std::is_void, list<char, short, int, void>>::type,
                 void>);
static_assert(
  std::is_same_v<find_t<std::is_void, list<char, short, int, void>>::index,
                 std::integral_constant<index_t, 4>>);

static_assert(
  std::is_same_v<
    list<void, void>,
    filter_t<std::is_void, list<void, char, int, void, float, double>>>);
static_assert(
  std::is_same_v<empty_list,
                 filter_t<std::is_void, list<char, int, float, double>>>);

static_assert(std::is_same_v<list<const void, const char, const int>,
                             map_t<std::add_const, list<void, char, int>>>);

static_assert(all_v<std::is_void, list<void, void, void>>);
static_assert(!all_v<std::is_void, list<void, int, double>>);
static_assert(any_v<std::is_void, list<void, int, double>>);
static_assert(!any_v<std::is_void, list<char, int, double>>);
static_assert(!all_v<std::is_void, list<char, int, double>>);
static_assert(none_v<std::is_void, list<char, int, double>>);
static_assert(all_v<std::is_void, empty_list>);
static_assert(!any_v<std::is_void, empty_list>);
static_assert(!none_v<std::is_void, empty_list>);

static_assert(
  std::integral_constant<int, 1>::value ==
  min_v<list<std::integral_constant<int, 1>, std::integral_constant<int, 2>>>);
static_assert(
  std::integral_constant<int, 1>::value ==
  min_v<list<std::integral_constant<int, 2>, std::integral_constant<int, 1>>>);
static_assert(
  std::integral_constant<int, 2>::value ==
  max_v<list<std::integral_constant<int, 1>, std::integral_constant<int, 2>>>);
static_assert(
  std::integral_constant<int, 2>::value ==
  max_v<list<std::integral_constant<int, 2>, std::integral_constant<int, 1>>>);

static_assert(
  std::is_same_v<list<std::integral_constant<std::size_t, 0>>,
                 make_integral_constants_t<std::make_index_sequence<1>>>);
static_assert(
  std::is_same_v<list<std::integral_constant<std::size_t, 0>,
                      std::integral_constant<std::size_t, 1>>,
                 make_integral_constants_t<std::make_index_sequence<2>>>);
static_assert(
  std::is_same_v<list<std::integral_constant<std::size_t, 0>,
                      std::integral_constant<std::size_t, 1>,
                      std::integral_constant<std::size_t, 2>>,
                 make_integral_constants_t<std::make_index_sequence<3>>>);
