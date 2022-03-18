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
#include "Signal.hpp"
#include "CheckedPOSIX.hpp"
#include "unreachable.hpp"

#include <exception>
#include <iostream>
#include <system_error>

namespace monomux
{

void SignalHandling::handler(Signal SigNum,
                             ::siginfo_t* Info,
                             void* /*Context*/)
{
  std::cout << SigNum << " received." << std::endl;

  if (static_cast<std::size_t>(SigNum) > SignalCount)
    unreachable("Unhandleable too large signal number received");

  SignalHandling* Context = Singleton.get();
  if (!Context)
    return;

  for (std::size_t I = 0; I < CallbackCount; ++I)
  {
    std::function<SignalCallback>& Cb = Context->Callbacks.at(SigNum).at(I);
    if (Cb)
      Cb(SigNum, Info, Context);
  }
}

std::unique_ptr<SignalHandling> SignalHandling::Singleton;

SignalHandling& SignalHandling::get()
{
  if (!Singleton)
    Singleton = std::make_unique<SignalHandling>();

  return *Singleton;
}

void SignalHandling::enable()
{
  for (std::size_t S = 0; S < SignalCount; ++S)
  {
    if (!Callbacks.at(S).at(0))
      // If the first element for the signal's callback is empty, no handling
      // is needed.
      continue;
    if (RegisteredSignals.at(S))
      // This signal is (assumed to be) already registered in the kernel.
      continue;

    POD<struct ::sigaction> SigAct;
    // TODO: Maybe extra flag support for SIGCHLD?
    SigAct->sa_flags = SA_SIGINFO;
    SigAct->sa_sigaction = &handler;

    CheckedPOSIXThrow([S, &SigAct] { return ::sigaction(S, &SigAct, nullptr); },
                      "sigaction(" + std::to_string(S) + ")",
                      -1);
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

    POD<struct ::sigaction> SigAct;
    SigAct->sa_handler = SIG_DFL;

    CheckedPOSIXThrow([S, &SigAct] { return ::sigaction(S, &SigAct, nullptr); },
                      "sigaction(" + std::to_string(S) + ", SIG_DFL)",
                      -1);
    RegisteredSignals.at(S) = true;
  }
}

void SignalHandling::registerCallback(Signal SigNum,
                                      std::function<SignalCallback> Callback)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  if (SigNum == SIGKILL || SigNum == SIGSTOP || SigNum == SIGSEGV)
    throw std::invalid_argument{"Signal " + std::to_string(SigNum) +
                                " cannot be handled!"};

  for (std::size_t I = 0; I < CallbackCount; ++I)
  {
    if (!Callbacks.at(SigNum).at(I))
    {
      Callbacks.at(SigNum).at(I) = std::move(Callback);
      return;
    }
  }

  throw std::out_of_range{"Signal " + std::to_string(SigNum) +
                          " callback vector is full (max size: " +
                          std::to_string(CallbackCount) + ")!"};
}

void SignalHandling::clearCallbacks(Signal SigNum)
{
  if (static_cast<std::size_t>(SigNum) > SignalCount)
    throw std::out_of_range{"Invalid signal " + std::to_string(SigNum)};

  for (std::size_t I = 0; I < CallbackCount; ++I)
  {
    std::function<SignalCallback> Empty{};
    Callbacks.at(SigNum).at(I).swap(Empty);
  }
}

void SignalHandling::clearCallbacks() noexcept
{
  for (std::size_t S = 0; S < SignalCount; ++S)
    for (std::size_t I = 0; I < CallbackCount; ++I)
    {
      std::function<SignalCallback> Empty{};
      Callbacks.at(S).at(I).swap(Empty);
    }
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
  {
    if (ObjectNames.at(I) == Name)
    {
      Objects.at(I).reset();
      return;
    }
  }
}

std::any* SignalHandling::getObject(const char* Name) noexcept
{
  return const_cast<std::any*>(
    const_cast<const SignalHandling*>(this)->getObject(Name));
}

const std::any* SignalHandling::getObject(const char* Name) const noexcept
{
  if (!Name)
    return nullptr;

  for (std::size_t I = 0; I < ObjectCount; ++I)
  {
    if (std::strncmp(
          ObjectNames.at(I).c_str(), Name, ObjectNames.at(I).size()) == 0)
    {
      return &Objects.at(I);
    }
  }

  return nullptr;
}

} // namespace monomux
