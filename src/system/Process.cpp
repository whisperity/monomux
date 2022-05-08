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
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iomanip>

#include <linux/limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include "monomux/adt/POD.hpp"
#include "monomux/system/CheckedPOSIX.hpp"
#include "monomux/system/Pty.hpp"
#include "monomux/unreachable.hpp"

#include "monomux/system/Process.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Process")

namespace monomux
{

static void allocCopyString(const std::string& Source,
                            char* DestinationStringArray[],
                            std::size_t Index)
{
  DestinationStringArray[Index] =
    reinterpret_cast<char*>(std::calloc(Source.size() + 1, 1));
  std::strncpy(
    DestinationStringArray[Index], Source.c_str(), Source.size() + 1);
}

Process::raw_handle Process::thisProcess()
{
  return CheckedPOSIXThrow([] { return ::getpid(); }, "getpid()", -1);
}

std::string Process::thisProcessPath()
{
  POD<char[PATH_MAX]> Binary;
  CheckedPOSIXThrow(
    [&Binary] { return ::readlink("/proc/self/exe", Binary, PATH_MAX); },
    "readlink(\"/proc/self/exe\")",
    -1);
  return {Binary};
}

[[noreturn]] void Process::exec(const SpawnOptions& Opts)
{
  LOG(debug) << "----- Process::exec() "
             << "was called -----";

  char** NewArgv = new char*[Opts.Arguments.size() + 2];
  allocCopyString(Opts.Program, NewArgv, 0);
  LOG(debug) << "        Program: " << Opts.Program;
  NewArgv[Opts.Arguments.size() + 1] = nullptr;
  for (std::size_t I = 0; I < Opts.Arguments.size(); ++I)
  {
    allocCopyString(Opts.Arguments[I], NewArgv, I + 1);
    LOG(debug) << "        Arg "
               << std::setw(log::Logger::digits(Opts.Arguments.size())) << I
               << ": " << Opts.Arguments[I];
  }

  for (const auto& E : Opts.Environment)
  {
    if (!E.second.has_value())
    {
      LOG(debug) << "        Env unset: " << E.first;
      CheckedPOSIX([&K = E.first] { return ::unsetenv(K.c_str()); }, -1);
    }
    else
    {
      LOG(debug) << "        Env   set: " << E.first << " = " << *E.second;
      CheckedPOSIX(
        [&K = E.first, &V = E.second] {
          return ::setenv(K.c_str(), V->c_str(), 1);
        },
        -1);
    }
  }

  if (Opts.CreatePTY)
    LOG(debug) << "        pty: Yes";
  else
  {
    if (Opts.StandardInput)
      LOG(debug) << "        stdin: " << *Opts.StandardInput;
    if (Opts.StandardOutput)
      LOG(debug) << "       stdout: " << *Opts.StandardOutput;
    if (Opts.StandardError)
      LOG(debug) << "       stderr: " << *Opts.StandardError;
  }

  LOG(debug) << "----- Process::exec() "
             << "firing... -----";

  if (!Opts.CreatePTY)
  {
    // Replaces the "Original" file descriptor with the new "With" one.
    auto ReplaceFD = [](raw_fd Original, raw_fd With) {
      if (With == fd::Invalid)
        CheckedPOSIX([Original] { return ::close(Original); }, -1);
      else
      {
        CheckedPOSIXThrow([=] { return ::dup2(With, Original); }, "dup2()", -1);
        CheckedPOSIX([With] { return ::close(With); }, -1);
      }
    };
    if (Opts.StandardInput)
      ReplaceFD(fd::fileno(stdin), *Opts.StandardInput);
    if (Opts.StandardError)
      ReplaceFD(fd::fileno(stderr), *Opts.StandardError);
    if (Opts.StandardOutput)
      ReplaceFD(fd::fileno(stdout), *Opts.StandardOutput);
  }

  auto ExecSuccessful =
    CheckedPOSIX([NewArgv] { return ::execvp(NewArgv[0], NewArgv); }, -1);
  if (!ExecSuccessful)
  {
    MONOMUX_TRACE_LOG(LOG(fatal)
                      << "'exec()' failed: " << ExecSuccessful.getError() << ' '
                      << ExecSuccessful.getError().message());
    std::_Exit(-SIGCHLD);
  }
  unreachable("::exec() should've started a new process");
}

Process Process::spawn(const SpawnOptions& Opts)
{
  std::optional<Pty> PTY;
  if (Opts.CreatePTY)
    PTY.emplace(Pty{});

  raw_handle ForkResult =
    CheckedPOSIXThrow([] { return ::fork(); }, "fork() failed in spawn()", -1);
  if (ForkResult != 0)
  {
    // We are in the parent.
    Process P;
    P.Handle = ForkResult;
    MONOMUX_TRACE_LOG(LOG(debug) << "PID " << P.Handle << " spawned.");

    if (PTY)
    {
      PTY->setupParentSide();
      P.PTY = std::move(PTY);
    }

    return P;
  }

  // We are in the child.
  CheckedPOSIXThrow([] { return ::setsid(); }, "setsid()", -1);
  if (PTY)
    PTY->setupChildrenSide();

  Process::exec(Opts);
  unreachable("Process::exec() should've replaced the process.");
}

static std::pair<bool, int> reapAndGetExitCode(Process::raw_handle PID,
                                               bool Block)
{
  if (PID == Process::Invalid)
    return {true, -1};

  POD<int> WaitStatus;
  auto ChangedPID = CheckedPOSIX(
    [&WaitStatus, PID, Block] {
      return ::waitpid(PID, &WaitStatus, !Block ? WNOHANG : 0);
    },
    -1);
  if (!ChangedPID)
  {
    std::error_code EC = ChangedPID.getError();
    if (EC == std::errc::no_child_process /* ECHILD */)
      return {false, 0};
    throw std::system_error{EC,
                            "waitpid(" + std::to_string(PID) + ", " +
                              std::to_string(Block) + ")"};
  }
  if (ChangedPID.get() == 0)
    // if WNOHANG was specified and one or more child(ren) specified by pid
    // exist, but have not yet changed state, then 0 is returned.
    return {false, 0};
  // on success, returns the process ID of the child whose state has changed
  if (ChangedPID.get() != PID)
    return {false, 0};

  MONOMUX_TRACE_LOG(LOG(trace) << "Successfully reaped child PID " << PID);
  if (WIFEXITED(WaitStatus))
    return {true, WEXITSTATUS(WaitStatus)};
  if (WIFSIGNALED(WaitStatus))
    return {true, -(WTERMSIG(WaitStatus))};

  return {false, 0};
}

bool Process::reapIfDead()
{
  std::pair<bool, int> DeadAndExit = reapAndGetExitCode(Handle, false);
  if (!DeadAndExit.first)
    return false;

  Dead = true;
  ExitCode = DeadAndExit.second;
  return true;
}

void Process::wait()
{
  if (Handle == Invalid)
    return;

  MONOMUX_TRACE_LOG(LOG(debug)
                    << "Waiting on child PID " << Handle << " to exit...");
  std::pair<bool, int> DeadAndExit = reapAndGetExitCode(Handle, true);
  Dead = true;
  ExitCode = DeadAndExit.second;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Process::signal(int Signal) { signal(Handle, Signal); }

void Process::signal(raw_handle Handle, int Signal)
{
  if (Handle == Invalid)
    return;

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Sending signal " << Signal << " to PID " << Handle);
  CheckedPOSIX([PGroup = -Handle, Signal] { return ::kill(PGroup, Signal); },
               -1);
}

} // namespace monomux

#undef LOG
