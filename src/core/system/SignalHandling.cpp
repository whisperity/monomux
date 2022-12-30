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
#include <cstring>
#include <system_error>

#include "monomux/system/SignalHandling.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Signal")

namespace monomux::system
{

std::unique_ptr<SignalHandling> SignalHandling::Singleton;

SignalHandling& SignalHandling::get()
{
  if (!Singleton)
  {
    Singleton = SignalHandling::create();
    MONOMUX_TRACE_LOG(LOG(debug)
                      << "Initialised at address" << ' ' << Singleton.get());
  }
  return *Singleton;
}

std::unique_ptr<SignalHandling> SignalHandling::create()
{
  return std::unique_ptr<SignalHandling>(new SignalHandling());
}

SignalHandling::SignalHandling()
{

  Callbacks.fill(CallbackArray{});
  for (auto& CallbackArray : Callbacks)
    CallbackArray.fill(Callback{});

  ObjectNames.fill(std::string{});
  Objects.fill(std::any{});
  RegisteredSignals.fill(false);
  MaskedSignals.fill(false);
}

void SignalHandling::enable()
{
  for (std::size_t S = 0; S < SignalCount; ++S)
  {
    if (!Callbacks.at(S).at(0))
      // If the callback for the signal is empty, no handling is needed.
      continue;
    if (RegisteredSignals.at(S))
      // This signal is (assumed to be) already registered in the kernel.
      continue;
    if (MaskedSignals.at(S))
      // Ignored signals will not be overwritten.
      continue;

    PlatformSpecificSignalTraits::setSignalHandled(S);
    RegisteredSignals.at(S) = true;
  }
}

void SignalHandling::disable()
{
  for (std::size_t S = 0; S < SignalCount; ++S)
  {
    if (!RegisteredSignals.at(S))
      // This signal is (assumed to be) not registered in the kernel.
      continue;
    if (MaskedSignals.at(S))
      // Ignored signals will not be overwritten.
      continue;

    PlatformSpecificSignalTraits::setSignalDefault(S);
    RegisteredSignals.at(S) = false;
  }
}

bool SignalHandling::enabled(Signal SigNum) const noexcept
{
  return RegisteredSignals.at(SigNum) || MaskedSignals.at(SigNum);
}

void SignalHandling::reset()
{
  clearCallbacks();
  deleteObjects();
  unignore();
  disable();
}

void SignalHandling::ignore(Signal SigNum)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  if (MaskedSignals.at(SigNum))
    // Already ignored signals will not be touched.
    return;

  PlatformSpecificSignalTraits::setSignalIgnored(SigNum);
  MaskedSignals.at(SigNum) = true;
}

void SignalHandling::unignore(Signal SigNum)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  if (!MaskedSignals.at(SigNum))
    // Not ignored signals will not be touched.
    return;

  if (RegisteredSignals.at(SigNum))
    PlatformSpecificSignalTraits::setSignalHandled(SigNum);
  else
    PlatformSpecificSignalTraits::setSignalDefault(SigNum);

  MaskedSignals.at(SigNum) = false;
}

void SignalHandling::unignore()
{
  for (std::size_t S = 0; S < SignalCount; ++S)
    unignore(S);
}

void SignalHandling::registerCallback(Signal SigNum,
                                      std::function<SignalCallback> Callback)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  CallbackArray& SCBs = Callbacks.at(SigNum);
  if (SCBs.at(CallbackCount - 1))
    throw std::out_of_range{
      "Signal " + std::to_string(SigNum) + " already has max " +
      std::to_string(CallbackCount) + " callbacks registered"};

  if constexpr (CallbackCount >= 2)
    for (std::size_t I = CallbackCount - 1; I != 0; --I)
      if (SCBs.at(I - 1))
        SCBs.at(I - 1).swap(SCBs.at(I));

  SCBs.at(0) = std::move(Callback);
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "New callback added for " << signalName(SigNum));
}

void SignalHandling::clearOneCallback(Signal SigNum)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  CallbackArray& SCBs = Callbacks.at(SigNum);
  if (!SCBs.at(0))
    throw std::out_of_range{"Signal " + std::to_string(SigNum) +
                            " has no callbacks registered"};

  Callback Cb;
  SCBs.at(0).swap(Cb);
  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Top callback cleared from " << signalName(SigNum));

  if constexpr (CallbackCount >= 2)
    for (std::size_t I = 0; I < CallbackCount - 1; ++I)
      if (SCBs.at(I + 1))
        SCBs.at(I + 1).swap(SCBs.at(I));
}

void SignalHandling::clearCallbacks(Signal SigNum)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  Callbacks.at(SigNum).fill(Callback{});

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Callbacks cleared from " << signalName(SigNum));
}

void SignalHandling::clearCallbacks() noexcept
{
  for (std::size_t S = 0; S < SignalCount; ++S)
  {
    std::function<SignalCallback> Empty{};
    Callbacks.at(S).fill(Callback{});
  }
  MONOMUX_TRACE_LOG(LOG(trace) << "All callbacks cleared");
}

void SignalHandling::defaultCallback(Signal SigNum)
{
  clearCallbacks(SigNum);

  if (RegisteredSignals.at(SigNum) || MaskedSignals.at(SigNum))
  {
    // Reset the default behaviour if the signal is currently being handled.
    RegisteredSignals.at(SigNum) = false;
    MaskedSignals.at(SigNum) = false;

    PlatformSpecificSignalTraits::setSignalDefault(SigNum);
  }
}

std::function<SignalHandling::SignalCallback>
SignalHandling::getCallback(Signal SigNum, std::size_t Index) const
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};
  if (Index > CallbackCount)
    throw std::out_of_range{"Invalid index " + std::to_string(Index) +
                            " >= " + std::to_string(CallbackCount)};
  return Callbacks.at(SigNum).at(Index);
}

std::function<SignalHandling::SignalCallback>
SignalHandling::getOneCallback(Signal SigNum) const
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};
  return Callbacks.at(SigNum).at(0);
}

void SignalHandling::registerObject(std::string Name, std::any Object)
{
  if (Name.empty())
    throw std::invalid_argument{"Name"};

  for (std::size_t I = 0; I < ObjectCount; ++I)
  {
    std::string& IName = ObjectNames.at(I);

    if (IName == Name || IName.empty())
    {
      MONOMUX_TRACE_LOG(LOG(trace)
                        << "Object " << '"' << Name << '"' << " registered "
                        << '(' << "ID: " << I << ')');

      if (IName.empty())
        IName = std::move(Name);
      Objects.at(I) = std::move(Object);
      return;
    }
  }

  throw std::out_of_range{"Maximum number of objects (" +
                          std::to_string(ObjectCount) +
                          ") registered already."};
}

void SignalHandling::deleteObject(const std::string& Name) noexcept
{
  if (Name.empty())
    return;

  for (std::size_t I = 0; I < ObjectCount; ++I)
    if (ObjectNames.at(I) == Name)
    {
      Objects.at(I).reset();
      MONOMUX_TRACE_LOG(LOG(trace) << "Object " << '"' << Name << '"' << ' '
                                   << '(' << "ID: " << I << ')' << " deleted");
      return;
    }
}

void SignalHandling::deleteObjects() noexcept
{
  for (std::size_t I = 0; I < ObjectCount; ++I)
  {
    if (ObjectNames.at(I).empty())
      continue;
    ObjectNames.at(I).clear();
    Objects.at(I).reset();
  }
  MONOMUX_TRACE_LOG(LOG(trace) << "Objects"
                               << " deleted");
}

const std::any* SignalHandling::getObject(const char* Name) const noexcept
{
  if (!Name)
    return nullptr;

  for (std::size_t I = 0; I < ObjectCount; ++I)
    if (!ObjectNames.at(I).empty() &&
        std::strncmp(
          ObjectNames.at(I).c_str(), Name, ObjectNames.at(I).size()) == 0)
      return &Objects.at(I);

  return nullptr;
}

} // namespace monomux::system

#undef LOG
