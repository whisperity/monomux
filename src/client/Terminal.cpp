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
#include <cassert>
#include <functional>

#include <sys/ioctl.h>
#include <termios.h>

#include "monomux/client/Client.hpp"
#include "monomux/system/CheckedPOSIX.hpp"
#include "monomux/system/Pipe.hpp"

#include "monomux/client/Terminal.hpp"

namespace monomux::client
{

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Terminal::Terminal(raw_fd InputStream, raw_fd OutputStream)
  : In(InputStream), Out(OutputStream), AssociatedClient(nullptr)
{
#ifndef NDEBUG
  MovedFromCheck = true;
#endif
}

void Terminal::engage()
{
  if (engaged())
    throw std::logic_error{"Already engaged."};

  OriginalTerminalSettings.reset();
  CheckedPOSIXThrow(
    [this] { return ::tcgetattr(In, &OriginalTerminalSettings); },
    "tcgetattr(): I/O is not a TTY?",
    -1);

  fd::addStatusFlag(In, O_NONBLOCK);

  POD<struct ::termios> NewSettings = OriginalTerminalSettings;
  NewSettings->c_iflag &=
    ~(IXON | IXOFF | ICRNL | INLCR | IGNCR | IMAXBEL | ISTRIP);
  NewSettings->c_iflag |= IGNBRK;
  NewSettings->c_oflag &= ~(OPOST | ONLCR | OCRNL | ONLRET);
  NewSettings->c_lflag &= ~(IEXTEN | ICANON | ECHO | ECHOE | ECHONL | ECHOCTL |
                            ECHOPRT | ECHOKE | ISIG);
  NewSettings->c_cc[VMIN] = 1;
  NewSettings->c_cc[VTIME] = 0;

  CheckedPOSIXThrow(
    [this, &NewSettings] { return ::tcsetattr(In, TCSANOW, &NewSettings); },
    "tcsetattr()",
    -1);

  // TODO: Clear the screen and implement a redraw request from serverside.

  Engaged = true;
}

void Terminal::disengage()
{
  if (!engaged())
    return;

  fd::removeStatusFlag(In, O_NONBLOCK);

  CheckedPOSIXThrow(
    [this] { return ::tcsetattr(In, TCSADRAIN, &OriginalTerminalSettings); },
    "tcsetattr()",
    -1);

  // TODO: Clear the screen.

  Engaged = false;
}

void Terminal::clientInput(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");

  if (Client.getInputFile() != Term->input())
    throw std::invalid_argument{"Client InputFD != Terminal input"};

  static constexpr std::size_t ReadSize = 1 << 10;
  bool Success;
  std::string Input = Pipe::read(Term->input(), ReadSize, &Success);
  if (!Success || Input.empty())
    return;

  Client.sendData(Input);
}

void Terminal::clientOutput(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");

  static constexpr std::size_t ReadSize = 1 << 10;
  std::string Output = Client.getDataSocket()->read(ReadSize);
  Pipe::write(Term->output(), Output);
}

void Terminal::clientEventReady(Terminal* Term, Client& Client)
{
  assert(Term->MovedFromCheck &&
         "Terminal object registered as callback was moved.");

  if (Term->WindowSizeChanged.get().load())
  {
    Size S = Term->getSize();
    Client.notifyWindowSize(S.Rows, S.Columns);
    Term->WindowSizeChanged.get().store(false);
  }
}

void Terminal::setupClient(Client& Client)
{
  if (AssociatedClient)
    releaseClient();

  Client.setInputFile(input());
  Client.setInputCallback(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientInput, this, std::placeholders::_1));
  Client.setDataCallback(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientOutput, this, std::placeholders::_1));
  Client.setExternalEventProcessor(
    // NOLINTNEXTLINE(modernize-avoid-bind)
    std::bind(&Terminal::clientEventReady, this, std::placeholders::_1));

  AssociatedClient = &Client;
}

void Terminal::releaseClient()
{
  if (!AssociatedClient)
    return;

  (*AssociatedClient).setDataCallback({});
  (*AssociatedClient).setInputCallback({});
  (*AssociatedClient).setExternalEventProcessor({});
  (*AssociatedClient).setInputFile(fd::Invalid);

  AssociatedClient = nullptr;
}

Terminal::Size Terminal::getSize() const
{
  POD<struct ::winsize> Raw;
  CheckedPOSIXThrow([this, &Raw] { return ::ioctl(In, TIOCGWINSZ, &Raw); },
                    "ioctl(0, TIOCGWINSZ /* get window size */);",
                    -1);
  Size S;
  S.Rows = Raw->ws_row;
  S.Columns = Raw->ws_col;
  return S;
}

void Terminal::notifySizeChanged() const noexcept
{
  WindowSizeChanged.get().store(true);
}

} // namespace monomux::client
