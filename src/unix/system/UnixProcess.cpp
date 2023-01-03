/* SPDX-License-Identifier: LGPL-3.0-only */
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iomanip>

#include <linux/limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"
#include "monomux/system/Process.hpp"
#include "monomux/system/UnixPty.hpp"
#include "monomux/system/fd.hpp"
#include "monomux/unreachable.hpp"

#include "monomux/system/UnixProcess.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/Process")

namespace monomux::system
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

Process::Raw Process::thisProcess()
{
  return CheckedErrnoThrow([] { return ::getpid(); }, "getpid()", -1);
}

std::string Process::thisProcessPath()
{
  POD<char[PATH_MAX]> Binary;
  CheckedErrnoThrow(
    [&Binary] { return ::readlink("/proc/self/exe", Binary, PATH_MAX); },
    "readlink(\"/proc/self/exe\")",
    -1);
  return {Binary};
}

[[noreturn]] void Process::exec(const SpawnOptions& Opts)
{
  using namespace monomux::system::unix;

  LOG(debug) << "----- Process::exec() "
             << "was called -----";

  const std::size_t ArgC = Opts.Arguments.size();
  char** NewArgv = new char*[ArgC + 2];
  allocCopyString(Opts.Program, NewArgv, 0);
  LOG(debug) << "        Program: " << Opts.Program;
  NewArgv[ArgC + 1] = nullptr;
  for (std::size_t I = 0; I < ArgC; ++I)
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
      CheckedErrno([&K = E.first] { return ::unsetenv(K.c_str()); }, -1);
    }
    else
    {
      LOG(debug) << "        Env   set: " << E.first << " = " << *E.second;
      CheckedErrno(
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
    auto ReplaceFD = [](fd::raw_fd Original, fd::raw_fd With) {
      if (With == fd::Traits::Invalid)
        CheckedErrno([Original] { return ::close(Original); }, -1);
      else
      {
        CheckedErrnoThrow([=] { return ::dup2(With, Original); }, "dup2()", -1);
        CheckedErrno([With] { return ::close(With); }, -1);
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
    CheckedErrno([NewArgv] { return ::execvp(NewArgv[0], NewArgv); }, -1);
  if (!ExecSuccessful)
  {
    MONOMUX_TRACE_LOG(LOG(fatal)
                      << "'exec()' failed: " << ExecSuccessful.getError() << ' '
                      << ExecSuccessful.getError().message());
    std::_Exit(-SIGCHLD);
  }
  unreachable("::exec() should've started a new process");
}

std::unique_ptr<Process> Process::spawn(const SpawnOptions& Opts)
{
  std::unique_ptr<Pty> PTY;
  if (Opts.CreatePTY)
    PTY = std::make_unique<unix::Pty>();

  Raw ForkResult =
    CheckedErrnoThrow([] { return ::fork(); }, "fork() failed in spawn()", -1);
  if (ForkResult != 0)
  {
    // We are in the parent.
    std::unique_ptr<Process> P = std::make_unique<unix::Process>();
    P->Handle = ForkResult;
    MONOMUX_TRACE_LOG(LOG(debug) << "PID " << P->Handle << " spawned.");

    if (PTY)
    {
      PTY->setupParentSide();
      P->PTY = std::move(PTY);
    }

    return P;
  }

  // We are in the child.
  CheckedErrnoThrow([] { return ::setsid(); }, "setsid()", -1);
  if (PTY)
    PTY->setupChildrenSide();

  Process::exec(Opts);
  unreachable("Process::exec() should've replaced the process.");
}

static std::pair<bool, int> reapAndGetExitCode(Process::Raw PID, bool Block)
{
  if (PID == PlatformSpecificProcessTraits::Invalid)
    return {true, -1};

  POD<int> WaitStatus;
  auto ChangedPID = CheckedErrno(
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

namespace unix
{

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
  if (Handle == PlatformSpecificProcessTraits::Invalid)
    return;

  MONOMUX_TRACE_LOG(LOG(debug)
                    << "Waiting on child PID " << Handle << " to exit...");
  std::pair<bool, int> DeadAndExit = reapAndGetExitCode(Handle, true);
  Dead = true;
  ExitCode = DeadAndExit.second;
}

// NOLINTNEXTLINE(readability-make-member-function-const)
void Process::signal(int Signal) { system::Process::signal(Handle, Signal); }

} // namespace unix

void Process::signal(Raw Handle, int Signal)
{
  if (Handle == PlatformSpecificProcessTraits::Invalid)
    return;

  MONOMUX_TRACE_LOG(LOG(trace)
                    << "Sending signal " << Signal << " to PID " << Handle);
  CheckedErrno([PGroup = -Handle, Signal] { return ::kill(PGroup, Signal); },
               -1);
}

} // namespace monomux::system

#undef LOG
