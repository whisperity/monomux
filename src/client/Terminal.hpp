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
#include <termios.h>

#include "adt/MovableAtomic.hpp"
#include "adt/unique_scalar.hpp"
#include "system/POD.hpp"
#include "system/fd.hpp"

namespace monomux::client
{

class Client;

class Terminal
{
public:
  /// A record containing the size information of the controlled terminal.
  struct Size
  {
    unsigned short Rows;
    unsigned short Columns;
  };

  Terminal(raw_fd InputStream, raw_fd OutputStream);

  /// Engages control over the current input and ouput terminal and sets it
  /// into the mode necesary for remote communication.
  void engage();

  bool engaged() const noexcept { return Engaged; }

  /// Disengage control over the current input and output terminal, resetting
  /// the default state.
  void disengage();

  /// Sets the current \p Terminal to be the terminal associated with \p Client.
  /// Data typed into the \p input() of this terminal will be considered input
  /// by the client, and data received by the client will be printed to
  /// \p output().
  void setupClient(Client& Client);

  Client* getClient() noexcept { return AssociatedClient; }
  const Client* getClient() const noexcept { return AssociatedClient; }

  /// Releases the associated client and turns off its callbacks from firing
  /// the handlers of the \p Terminal.
  ///
  /// \see setupClient
  void releaseClient();

  raw_fd input() const noexcept { return In; }
  raw_fd output() const noexcept { return Out; }

  Size getSize() const;
  void notifySizeChanged() const noexcept;

private:
  unique_scalar<raw_fd, fd::Invalid> In;
  unique_scalar<raw_fd, fd::Invalid> Out;
  unique_scalar<Client*, nullptr> AssociatedClient;
  unique_scalar<bool, false> Engaged;
  POD<struct ::termios> OriginalTerminalSettings;

  /// Whether a signal interrupt indicated that the window size of the client
  /// had changed.
  mutable MovableAtomic<bool> WindowSizeChanged;

#ifndef NDEBUG
  /// The handler callbacks in the client receive a \p Terminal instance's
  /// pointer bound. If the object is moved from, the moved-from will reset
  /// this value to \p false, with which we can track the access of an invalid
  /// object with sanitisers.
  unique_scalar<bool, false> MovedFromCheck;
#endif

  /// Callback function fired when the client reports available input.
  static void clientInput(Terminal* Term, Client& Client);
  /// Callback function fired when the client reports available output.
  static void clientOutput(Terminal* Term, Client& Client);
  /// Callback function fired when the client is ready to process events of the
  /// environment.
  static void clientEventReady(Terminal* Term, Client& Client);
};

} // namespace monomux::client
