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
#include <cerrno>
#include <cstring>
#include <system_error>
#include <type_traits>
#include <utility>

namespace monomux
{

// Hardcode this, because decltype(errno) would not be portable.
// errno might be defined only as a macro!
using errno_t = int;

namespace detail
{

template <typename R> struct Result
{
private:
  R Value;
  bool Errored;
  std::error_code ErrorCode;

public:
  Result(R&& Value, bool Errored, std::error_code Error)
    : Value(std::move(Value)), Errored(Errored), ErrorCode(Error)
  {}

  explicit operator bool() const noexcept { return !Errored; }
  std::error_code getError() const noexcept { return ErrorCode; }
  R& get() noexcept { return Value; }
  const R& get() const noexcept { return Value; }
};

template <> struct Result<void>
{
private:
  bool Errored;
  std::error_code ErrorCode;

public:
  Result(bool Errored, std::error_code Error)
    : Errored(Errored), ErrorCode(Error)
  {}

  explicit operator bool() const noexcept { return !Errored; }
  std::error_code getError() const noexcept { return ErrorCode; }
};

} // namespace detail

/// Allows executing a system call with automatically handled \p errno checking.
///
/// Clients MUST pass a lambda that returns the value of the system call, and
/// list ALL the values which might indicate a FAILED system call.
///
/// The result of the call itself is obtainable from the return value of this
/// function.
///
/// Example:
///
///   \code{.cpp}
///   auto Open = CheckedErrno([]() {
///     return ::open("foo", O_RDONLY);
///   }, /* ErrorIndicatingReturnValue =*/-1);
///
///   if (!Open) {
///     // Do something as the call failed.
///   }
///   Open.get(); // Obtain the return value from the lambda.
///   \endcode
template <typename Fn, typename... ErrTys>
decltype(auto)
// NOLINTNEXTLINE(readability-identifier-naming)
CheckedErrno(Fn&& F, ErrTys&&... ErrorValues) noexcept
{
  using namespace monomux::detail;
  static_assert(!std::is_same_v<decltype(F()), void>,
                "Lambda must return something!");

  auto ReturnValue = F();
  bool Errored = (false || ... || (ReturnValue == ErrorValues));
  return Result<decltype(ReturnValue)>{
    std::move(ReturnValue),
    Errored,
    std::make_error_code(static_cast<std::errc>(errno))};
}

/// Allows executing a system call with translating an error to an exception.
///
/// Clients MUST pass a lambda that returns the value of the system call, and
/// list ALL the values which might indicate a FAILED system call.
///
/// The result of the call itself is returned by this function.
/// If the call fails, this function throws an exception.
///
/// Example:
///
///   \code{.cpp}
///   auto FD = CheckedErrnoThrow([]() {
///     return ::open("foo", O_RDONLY);
///   }, "Failed to open the file"s,
///   /* ErrorIndicatingReturnValue =*/-1);
///
///   FD; // Obtain the return value from the lambda.
///   \endcode
template <typename Fn, typename... ErrTys>
decltype(auto)
// NOLINTNEXTLINE(readability-identifier-naming)
CheckedErrnoThrow(Fn&& F, std::string ErrMsg, ErrTys&&... ErrorValues)
{
  auto Result =
    CheckedErrno(std::forward<Fn>(F), std::forward<ErrTys>(ErrorValues)...);
  if (!Result)
    throw std::system_error{Result.getError(), ErrMsg};

  // Make sure to not return an `int &` or something similar dangling!
  std::remove_reference_t<decltype(Result.get())> Copy = Result.get();
  return Copy;
}

/// Allows executing a system call with automatically handled \p errno checking.
///
/// This function allows for complex logic inside the callback, and complex
/// indication of errors.
///
/// Clients are encouraged to pass a lambda to which does the system call.
/// This lambda MUST take a single parameter, `bool& Error`, which the
/// caller MUST set to \p true if the syscall returned an error, appropriately.
/// The lambda MIGHT return additional values, which are obtainable from the
/// result of this call.
///
/// Example:
///
///   \code{.cpp}
///   auto Open = CheckedErrno([](bool& Error) {
///     int fd = ::open("foo", O_RDONLY);
///     Error = (fd == -1);
///     return 42;
///   });
///
///   if (!Open) {
///     // Do something as the call failed.
///   }
///   Open.get(); // Obtain the return value from the lambda.
///   \endcode
template <typename Fn>
decltype(auto)
CheckedErrno(Fn&& F) noexcept // NOLINT(readability-identifier-naming)
{
  using namespace monomux::detail;
  bool Errored = false;
  using result_type = decltype(F(Errored)); // What is the lambda returning?

  if constexpr (std::is_same_v<result_type, void>)
  {
    F(Errored);
    return Result<void>{Errored,
                        std::make_error_code(static_cast<std::errc>(errno))};
  }
  else
  {
    result_type R = F(Errored);
    return Result<result_type>{
      std::move(R),
      Errored,
      std::make_error_code(static_cast<std::errc>(errno))};
  }
}

} // namespace monomux
