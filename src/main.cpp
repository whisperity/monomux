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
#include "ExitCode.hpp"

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

static const char* ShortOptions = "hs:n:ldDN";
// clang-format off
static struct ::option LongOptions[] = { // NOLINT(modernize-avoid-c-arrays)
  {"help",       no_argument,       nullptr, 'h'},
  {"server",     no_argument,       nullptr, 0},
  {"socket",     required_argument, nullptr, 's'},
  {"name",       required_argument, nullptr, 'n'},
  {"list",       no_argument,       nullptr, 'l'},
  {"detach",     no_argument,       nullptr, 'd'},
  {"detach-all", no_argument,       nullptr, 'D'},
  {nullptr,      0,                 nullptr, 0}
};
// clang-format on

static void printOptionHelp()
{
  std::cout << R"EOF(Usage:
    monomux [SERVER OPTIONS...] --server
    monomux [CLIENT OPTIONS...] [PROGRAM]
    monomux [CLIENT OPTIONS...] -- PROGRAM [ARGS...]
    monomux (-dD)

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
    --server                    - Start the Monomux server explicitly, without
                                  creating a client, or any sessions. (This
                                  option should seldom be given by users.)


Client options:
    PROGRAM [ARGS...]           - If the session specified by '-n' does not
                                  exist, MonoMux will create a new session, in
                                  which the PROGRAM binary (with ARGS... given
                                  as its command-line arguments) will be
                                  started.

                                  It is recommended to specify a shell as the
                                  program. Defaults to the user's default shell
                                  (SHELL environment variable), "/bin/bash", or
                                  "/bin/sh", in this order.

                                  If the arguments to be passed to the started
                                  program start with '-' or '--', the program
                                  invocation and MonoMux's arguments must be
                                  separated by an explicit '--':

                                      monomux -n session /bin/zsh

                                      monomux -n session -- /bin/bash --no-rc

    -n NAME, --name NAME        - Name of the remote session to attach to, or
                                  create. (Defaults to: "default".)
                                  This option makes '--list' inoperative.
    -l, --list                  - Always start the client with the session list,
                                  even if only at most one session exists on the
                                  server. (The default behaviour is to
                                  automatically create a session or attach in
                                  this case.)
                                  This option makes '--name' inoperative.
    -s PATH, --socket PATH      - Path of the server socket to connect to.


In-session options:
    -d, --detach                - When executed from within a running session,
                                  detach the current client.
    -D, --detach-all            - When executed from within a running session,
                                  detach all clients attached to that session.


Server options:
    -s PATH, --socket PATH      - Path of the sever socket to create and await
                                  clients on.
    -N                          - Do not daemonise (put the running server into
                                  the background) automatically.

)EOF";
  std::cout << std::endl;
}

int main(int ArgC, char* ArgV[])
{
  server::Options ServerOpts{};
  client::Options ClientOpts{};

  // ------------------------ Parse command-line options -----------------------
  {
    bool HadErrors = false;
    int Opt;
    int LongOptIndex;
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
          {
            ServerOpts.ServerMode = true;
            ClientOpts.ClientMode = false;
          }
          else
          {
            std::cerr << ArgV[0] << ": option '--" << Opt
                      << "' registered, but no handler associated with it"
                      << std::endl;
            return -1;
          }
          break;
        }
        case '?':
          HadErrors = true;
          break;
        case 'h':
          printOptionHelp();
          return EXIT_Success;
        case 's':
          ClientOpts.SocketPath.emplace(optarg);
          break;
        case 'n':
          ClientOpts.SessionName.emplace(optarg);
          break;
        case 'l':
          ClientOpts.ForceSessionSelectMenu = true;
          break;
        case 'd':
          ClientOpts.DetachRequestLatest = true;
          break;
        case 'D':
          ClientOpts.DetachRequestAll = true;
          break;
        case 'N':
          ServerOpts.Background = false;
          break;
      }
    }

    if (ClientOpts.DetachRequestLatest && ClientOpts.DetachRequestAll)
    {
      std::cerr << ArgV[0]
                << ": option '-D/--detach-all' and '-d/--detach' are mutually "
                   "exclusive!"
                << std::endl;
      HadErrors = true;
    }

    if (!ServerOpts.ServerMode)
      ClientOpts.ClientMode = true;

    // Handle positional arguments not handled earlier.
    for (; ::optind < ArgC; ++::optind)
    {
      if (ServerOpts.ServerMode)
      {
        std::cerr << "ERROR: '--server' does not take positional argument \""
                  << ArgV[::optind] << "\"" << std::endl;
        HadErrors = true;
        break;
      }

      assert(ClientOpts.ClientMode);
      if (!ClientOpts.Program.has_value())
        // The first positional argument is the program name to spawn.
        ClientOpts.Program.emplace(ArgV[::optind]);
      else
        // Otherwise they are arguments to the program to start.
        ClientOpts.ProgramArgs.emplace_back(ArgV[::optind]);
    }

    if (HadErrors)
      return EXIT_InvocationError;
  }

  // --------------------- Set up some internal environment --------------------
  {
    if (ClientOpts.isControlMode() && !ClientOpts.SocketPath)
    {
      // Load a session from the current process's environment, to have a socket
      // for the controller client ready, if needed.
      std::optional<MonomuxSession> Sess = MonomuxSession::loadFromEnv();
      if (Sess)
      {
        ClientOpts.SocketPath = Sess->Socket.toString();
        ClientOpts.SessionData = std::move(Sess);
      }
    }

    SocketPath SocketPath = ClientOpts.SocketPath.has_value()
                              ? SocketPath::absolutise(*ClientOpts.SocketPath)
                              : SocketPath::defaultSocketPath();
    ClientOpts.SocketPath = SocketPath.toString();
    ServerOpts.SocketPath = ClientOpts.SocketPath;

    std::clog << "DEBUG: Using socket " << *ClientOpts.SocketPath << std::endl;
  }

  // --------------------- Dispatch to appropriate handler ---------------------
  if (ServerOpts.ServerMode)
    return server::main(ServerOpts);

  // The default behaviour in the client is to always try establishing a
  // connection to a server. However, it is very likely that the current process
  // has been the first monomux instance created by the user, in which case
  // there will be no server running. For convenience, we can initialise a
  // server right here.
  {
    std::string FailureReason;
    std::optional<client::Client> ToServer;
    try
    {
      ToServer = client::connect(ClientOpts, false, &FailureReason);
    }
    catch (...)
    {}

    if (!ToServer)
    {
      std::clog << "DEBUG: No running server found, creating one..."
                << std::endl;

      ServerOpts.ServerMode = true;
      Process::fork(
        [] { /* In the parent, continue. */
             return;
        },
        [&ServerOpts, &ArgV] /* NOLINT(modernize-avoid-c-arrays) */ {
          // Perform the server restart in the child, so it gets
          // disowned when we eventually exit, and we can remain the
          // client.
          server::exec(ServerOpts, ArgV[0]);
        });

      // Give some time for the server to spawn...
      std::this_thread::sleep_for(std::chrono::seconds(1));

      try
      {
        ToServer = client::connect(ClientOpts, true, &FailureReason);
      }
      catch (...)
      {}
    }

    if (!ToServer)
    {
      std::cerr << "ERROR: Connecting to the MonoMux server failed:\n\t"
                << FailureReason << std::endl;
      return EXIT_SystemError;
    }

    ClientOpts.Connection = std::move(ToServer);
  }

  return client::main(ClientOpts);
}
