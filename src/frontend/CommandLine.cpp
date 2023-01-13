/* SPDX-License-Identifier: GPL-3.0-only */
#include <cassert>
#include <stdexcept>

#include "monomux/CommandLine.hpp"
#include "monomux/adt/Ranges.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("CommandLine")

namespace monomux
{

namespace cli
{

void Option::addOptionName(std::string_view Name)
{
  if (Name.empty())
    throw std::invalid_argument{"Option name must be specified"};
  if (Name.front() == '-' || Name.front() == '/' || Name.front() == ':')
    throw std::invalid_argument{
      "Option name must not start with the special character that begins an "
      "option at invocation. The system adds this automatically!"};

  if (ranges::any_of(
        Names, [Name](const auto& KnownName) { return KnownName == Name; }))
    return;
  Names.emplace_back(Name);
}


} // namespace cli

void CommandLineInterface::addOption(cli::Option* O)
{
  assert(O);
  for (const std::string& Name : O->getNames())
  {
    if (Options.find(Name) != Options.end())
      throw std::out_of_range{"Trying to register an option with name '" +
                              Name + "' but such is already registered."};
    Options[Name] = O;
  }
}

void devstuff()
{
  using namespace monomux::cli;

  ValueOption<void> O{"foo", "bar"};

  CommandLineInterface CLI;
  CLI.addOption(&O);
}

} // namespace monomux

#undef LOG
