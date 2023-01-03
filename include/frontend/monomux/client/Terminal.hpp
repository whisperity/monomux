/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <termios.h>

#include "monomux/adt/Atomic.hpp"
#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/adt/UniqueScalar.hpp"
#include "monomux/system/Handle.hpp"
#include "monomux/system/Pipe.hpp"

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

  Terminal(system::Handle::Raw InputStream, system::Handle::Raw OutputStream);

  /// Engages control over the current input and ouput terminal and sets it
  /// into the mode necesary for remote communication.
  void engage();

  [[nodiscard]] bool engaged() const noexcept { return Engaged; }

  /// Disengage control over the current input and output terminal, resetting
  /// the default state.
  void disengage();

  /// Sets the current \p Terminal to be the terminal associated with \p Client.
  /// Data typed into the \p input() of this terminal will be considered input
  /// by the client, and data received by the client will be printed to
  /// \p output().
  void setupClient(Client& Client);

  [[nodiscard]] const Client* getClient() const noexcept
  {
    return AssociatedClient;
  }
  MONOMUX_MEMBER_0(Client*, getClient, [[nodiscard]], noexcept);

  /// Releases the associated client and turns off its callbacks from firing
  /// the handlers of the \p Terminal.
  ///
  /// \see setupClient
  void releaseClient();

  [[nodiscard]] const system::Pipe* input() const noexcept { return In.get(); }
  MONOMUX_MEMBER_0(system::Pipe*, input, [[nodiscard]], noexcept);
  [[nodiscard]] const system::Pipe* output() const noexcept
  {
    return Out.get();
  }
  MONOMUX_MEMBER_0(system::Pipe*, output, [[nodiscard]], noexcept);

private:
  std::unique_ptr<system::Pipe> In;
  std::unique_ptr<system::Pipe> Out;
  UniqueScalar<Client*, nullptr> AssociatedClient;
  UniqueScalar<bool, false> Engaged;
  POD<struct ::termios> OriginalTerminalSettings;

#ifndef NDEBUG
  /// The handler callbacks in the client receive a \p Terminal instance's
  /// pointer bound. If the object is moved from, the moved-from will reset
  /// this value to \p false, with which we can track the access of an invalid
  /// object with sanitisers.
  // FIXME: Refactor this into an ADT entity.
  UniqueScalar<bool, false> MovedFromCheck;
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
