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
#include "client/Main.hpp"
#include "server/Main.hpp"
#include "server/Server.hpp"
#include "system/CheckedPOSIX.hpp"
#include "system/Environment.hpp"
#include "system/Process.hpp"
#include "system/Pty.hpp"
#include "system/unreachable.hpp"

#include <chrono>
#include <iostream>
#include <system_error>
#include <thread>

#include <getopt.h>
#include <unistd.h>

using namespace monomux;

static const char* ShortOptions = "h";
// clang-format off
static struct ::option LongOptions[] = {
  {"help",   no_argument, 0, 'h'},
  {"server", no_argument, 0, 0},
  {0, 0, 0, 0}
};
// clang-format on

static void printOptionHelp()
{
  using std::cout, std::endl;

  cout << R"EOF(Usage: monomux [OPTIONS...] [PROGRAM...]

                 MonoMux -- Monophone Terminal Multiplexer

MonoMux is a system tool that allows executing shell sessions and processes in
a separate session in the background, and allows multiple clients attach to the
sessions.

Shells and programs are executed by a server that is automatically created for
the user at the first interaction. The client program (started by default when
monomux is called) takes over the user's terminal and communicates data to and
from the shell or program running under the server. This way, if the client
exits (either because the user explicitly requested it doing so, or through a
SIGHUP signal, e.g. in the case of SSH), the remote process may still continue
execution in the background.

NOTE! Unlike other terminal session manager or multiplexer tools, such as screen
or tmux, MonoMux performs NO VT-SEQUENCE (the invisible control characters that
make an interactive terminal an enjoyable experience) PARSING or understanding!
To put it bluntly, MonoMux is **NOT A TERMINAL EMULATOR**! Data from the
underlying program is passed verbatim to the attached client(s).

Options:
    --server        Start the Monomux server explicitly, without creating a
                    default session. (This option should seldom be given by
                    users.)
)EOF";
  cout << endl;
}

int main(int ArgC, char* ArgV[])
{
  server::Options ServerOpts;
  client::Options ClientOpts;

  // ------------------------ Parse command-line options -----------------------
  {
    int Opt, LongOptIndex;
    while ((Opt = ::getopt_long(
              ArgC, ArgV, ShortOptions, LongOptions, &LongOptIndex)) != -1)
    {
      switch (Opt)
      {
        case 0:
        {
          // Long-option was specified.
          std::string_view Opt = LongOptions[LongOptIndex].name;
          if (Opt == "server")
            ServerOpts.ServerMode = true;
          else
          {
            std::cerr << ArgV[0] << ": option '--" << Opt
                      << "' registered, but no handler associated with it"
                      << std::endl;
            return -1;
          }
          break;
        }
        case 'h':
          printOptionHelp();
          return EXIT_SUCCESS;
      }
    }

    for (; ::optind < ArgC; ++::optind)
    {
      std::cout << "Remaining argument: " << ArgV[::optind] << std::endl;
    }
  }

  // --------------------- Dispatch to appropriate handler ---------------------
  if (ServerOpts.ServerMode)
    return server::main(ServerOpts);

  // Assume client mode, if no options were present.
  ClientOpts.ClientMode = true;

  // The default behaviour in the client is to always try establishing a
  // connection to a server. However, it is very likely that the current process
  // has been the first monomux instance created by the user, in which case
  // there will be no server running. For convenience, we can initialise a
  // server right here.
  {
    std::optional<Client> ToServer = client::connect(ClientOpts, false);
    if (!ToServer)
    {
      // TODO: Work out how this would work with signals and the TTY of the
      // client.
      std::cerr << "ERROR: No running Monomux server found. Please start one "
                   "manually with `monomux --server` for now!"
                << std::endl;
      return EXIT_FAILURE;

      std::clog << "DEBUG: No running server found, creating one..."
                << std::endl;

      ServerOpts.ServerMode = true;
      Process::fork(
        [] { /* In the parent, continue. */
             return;
        },
        [&ServerOpts, &ArgV] {
          // Perform the server restart in the child, so it gets
          // disowned when we eventually exit, and we can remain the
          // client.
          server::exec(ServerOpts, ArgV[0]);
        });

      ToServer = client::connect(ClientOpts, true);
    }

    ClientOpts.Connection = std::move(ToServer);
  }

  return client::main(ClientOpts);
}
