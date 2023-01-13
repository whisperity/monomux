/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace monomux
{

namespace cli
{

namespace detail
{

template <class Feature> struct Apply
{
  // template <class Option> static void apply(Option& O, Feature&& F)
  // {
  //   F.apply(O);
  // }
};

template <class Option> void apply(Option& /*O*/)
{ /* Noop. */
}

template <class Option, class Feat, class... Feats>
void apply(Option& O, Feat&& F, Feats&&... Fs)
{
  Apply<Feat>::apply(O, std::forward<Feat>(F));
  apply(O, std::forward<Feats>(Fs)...);
}

/* NOLINTBEGIN(bugprone-macro-parantheses) */
#define MONOMUX_CUSTOM_APPLICATOR(                                             \
  OUTER_TPARAM, SPECIALISATION_TARG, FUNCTION_ARG, BODY)                       \
  template <OUTER_TPARAM> struct Apply<SPECIALISATION_TARG>                    \
  {                                                                            \
    template <class Option> static void apply(Option& O, FUNCTION_ARG F)       \
    {                                                                          \
      BODY;                                                                    \
    }                                                                          \
  };
/* NOLINTEND(bugprone-macro-parantheses) */

MONOMUX_CUSTOM_APPLICATOR(std::size_t N,
                          const char (&)[N],
                          std::string_view,
                          O.addOptionName(F));
MONOMUX_CUSTOM_APPLICATOR(,
                          std::string_view,
                          std::string_view,
                          O.addOptionName(F));
MONOMUX_CUSTOM_APPLICATOR(, std::string, std::string_view, O.addOptionName(F));

#undef MONOMUX_CUSTOM_APPLICATOR

} // namespace detail

class Option
{
  std::vector<std::string> Names;

public:
  /// Prepares the current option to be also available under \p Name.
  /// The \p Name must be provided without a prefix - it is auto-applied by
  /// the system at usage points.
  void addOptionName(std::string_view Name);

  [[nodiscard]] const std::vector<std::string>& getNames() const noexcept
  {
    return Names;
  }
};

#define MONOMUX_APPLICABLE_OPTION_CTOR(NAME)                                   \
  template <class... Feats> explicit NAME(Feats&&... Fs) : Option()            \
  {                                                                            \
    detail::apply(*this, std::forward<Feats>(Fs)...);                          \
  }

/// ???
template <typename T> class ValueOption : public Option
{
public:
  MONOMUX_APPLICABLE_OPTION_CTOR(ValueOption);
};

#undef MONOMUX_APPLICABLE_OPTION_CTOR

} // namespace cli

/// Contains the parsing logic required to meaningfully interface with a set of
/// command-line arguments (\p cli::Option) to obtain configuration from the
/// user's invocation.
class CommandLineInterface
{
  std::map<std::string, cli::Option*> Options;

public:
  /// Registers the option \p O to be handled by the current instance.
  /// Only a \b WEAK reference is stored, the execution of the parser
  /// \e mutates the \p O instance itself.
  void addOption(cli::Option* O);
};

} // namespace monomux
