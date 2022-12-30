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
#include <cstdint>
#include <vector>

#include <sys/ioctl.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/system/SignalHandling.hpp"

#include "monomux/system/UnixTerminal.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("unix/Terminal")

namespace monomux::system::unix
{

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::shared_ptr<Terminal> Terminal::create(fd::raw_fd In, fd::raw_fd Out)
{
  return std::shared_ptr<Terminal>(new Terminal{In, Out});
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Terminal::Terminal(fd::raw_fd In, fd::raw_fd Out) : In(In), Out(Out)
{
  CheckedErrnoThrow([this] { return ::tcgetattr(this->In, &TerminalSettings); },
                    "tcgetattr(" + std::to_string(this->In) +
                      "): I/O is not a TTY?",
                    -1);
}

void Terminal::setRawMode()
{
  fd::setNonBlocking(In);

  POD<struct ::termios> NewSettings = TerminalSettings;
  NewSettings->c_iflag &=
    ~(IXON | IXOFF | ICRNL | INLCR | IGNCR | IMAXBEL | ISTRIP);
  NewSettings->c_iflag |= IGNBRK;
  NewSettings->c_oflag &= ~(OPOST | ONLCR | OCRNL | ONLRET);
  NewSettings->c_lflag &= ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | ECHOCTL |
                            ECHOPRT | ECHOKE | ISIG);
  NewSettings->c_cc[VMIN] = 1;
  NewSettings->c_cc[VTIME] = 0;
  NewSettings->c_cflag |= CS8;

  CheckedErrnoThrow(
    [this, &NewSettings] { return ::tcsetattr(In, TCSANOW, &NewSettings); },
    "tcsetattr(" + std::to_string(In) +
      "TCSANOW /* Change now */, NewSettings)",
    -1);
}

void Terminal::setOriginalMode()
{
  fd::setBlocking(In);

  CheckedErrnoThrow(
    [this] { return ::tcsetattr(In, TCSADRAIN, &TerminalSettings); },
    "tcsetattr(" + std::to_string(In) +
      "TCSADRAIN /* Change after outputs */, OldSettings)",
    -1);
}

Terminal::Size Terminal::getSize() const
{
  POD<struct ::winsize> Raw;
  CheckedErrnoThrow([this, &Raw] { return ::ioctl(In, TIOCGWINSZ, &Raw); },
                    "ioctl(" + std::to_string(In) +
                      ", TIOCGWINSZ /* get window size */);",
                    -1);
  return Size{Raw->ws_row, Raw->ws_col};
}

namespace terminal
{
namespace
{

using LookupValue = std::weak_ptr<Terminal>;
using Lookup = std::vector<LookupValue>;
constexpr char LookupName[] = "UnixTerminals";

Lookup::const_iterator getMaybeInstanceIter(const Lookup& Lookup,
                                            Terminal* This)
{
  return std::find_if(
    Lookup.begin(), Lookup.end(), [This](const LookupValue& MaybeInstance) {
      if (auto Instance = MaybeInstance.lock())
        if (Instance.get() == This)
          return true;
      return false;
    });
}

/// Handler for \p SIGWINCH (window size change) events produced by the terminal
/// the program is running in.
MONOMUX_SIGNAL_HANDLER(windowSizeChanged)
{
  (void)Sig;
  (void)PlatformInfo;

  const auto* Terms = SignalHandling->getObjectAs<Lookup>(LookupName);
  if (!Terms)
    return;
  for (const LookupValue& MaybeInstance : *Terms)
    if (auto Instance = MaybeInstance.lock())
      Instance->notifySizeChanged();

  LOG(always) << "\r\n(Developer note) SIGWINCH event captured, but the sink "
                 "doesn't do anything wih it!\r\n";
}

} // namespace
} // namespace terminal

void Terminal::setupListenForSizeChangeSignal(system::SignalHandling& Handling)
{
  using namespace monomux::system::unix::terminal;

  if (!Handling.getObject(LookupName))
  {
    // Register a data structure that allows fetching for the Terminal's
    // associated input device the Terminal instance. As we are now in the
    // platform-specific library, this might *NOT* be global state, so we
    // must not invalidate the contents of potentially already existing
    // signal handlers...
    Handling.registerObject(LookupName, Lookup{});
    Handling.registerCallback(SIGWINCH, &windowSizeChanged);
  }

  auto* MaybeLookup = Handling.getObjectAs<Lookup>(LookupName);
  assert(MaybeLookup && "If lookup was missing, it must not by now!");
  Lookup& Lookup = *MaybeLookup;

  if (getMaybeInstanceIter(Lookup, this) != Lookup.end())
    // Already listening...
    return;

  Lookup.emplace_back(weak_from_this());
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Terminal::teardownListenForSizeChangeSignal(
  system::SignalHandling& Handling)
{
  using namespace monomux::system::unix::terminal;

  if (!Handling.getObject(LookupName))
    // startListenForSizeChangeSignal() was never called.
    return;

  auto* MaybeLookup = Handling.getObjectAs<Lookup>(LookupName);
  assert(MaybeLookup && "If lookup was missing, it must not by now!");
  Lookup& Lookup = *MaybeLookup;

  if (getMaybeInstanceIter(Lookup, this) == Lookup.end())
    // Currently was not listening for 'this'.
    return;

  Lookup.erase(std::remove_if(Lookup.begin(),
                              Lookup.end(),
                              [this](const LookupValue& V) {
                                return V.lock() == nullptr ||
                                       V.lock().get() == this;
                              }),
               Lookup.end());

  if (Lookup.empty())
  {
    // If the current object was the last one to be removed, perform some
    // cleanup.
    Handling.deleteObject(LookupName);
    Handling.defaultCallback(SIGWINCH);
  }
}

} // namespace monomux::system::unix

#undef LOG
