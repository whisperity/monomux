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
#include "monomux/CheckedErrno.hpp"
#include "monomux/adt/POD.hpp"

#include "monomux/system/SignalHandling.hpp"
#include "monomux/system/UnixSignalTraits.hpp"

#include "monomux/Log.hpp"
#define LOG(SEVERITY) monomux::log::SEVERITY("system/UnixSignal")

namespace monomux::system
{

const char* SignalHandling::signalName(SignalHandling::Signal S) noexcept
{
#ifdef MONOMUX_NON_ESSENTIAL_LOGS
  switch (S)
  {
    case SIGINT:
      return "SIGINT (Interrupted)";
    case SIGILL:
      return "SIGILL (Illegal instruction)";
    case SIGABRT:
      return "SIGABRT(/SIGIOT) (Aborted)";
    case SIGFPE:
      return "SIGFPE (Floating-point exception)";
    case SIGSEGV:
      return "SIGSEGV (Segmentation fault)";
    case SIGTERM:
      return "SIGTERM (Termination)";
    case SIGHUP:
      return "SIGHUP (Hung up)";
    case SIGQUIT:
      return "SIGQUIT (Quit)";
    case SIGTRAP:
      return "SIGTRAP (Trace trapped)";
    case SIGKILL:
      return "SIGKILL (Killed)";
    case SIGBUS:
      return "SIGBUS (Bus error)";
    case SIGSYS:
      return "SIGSYS (Bad system call)";
    case SIGPIPE:
      return "SIGPIPE (Broken pipe)";
    case SIGALRM:
      return "SIGALRM (Timer tocked)";
    case SIGURG:
      return "SIGURG (Urgent data on socket)";
    case SIGSTOP:
      return "SIGSTOP (Stop process)";
    case SIGTSTP:
      return "SIGTSTP (Terminal stop request)";
    case SIGCONT:
      return "SIGCONT (Continue)";
    case SIGCHLD:
      return "SIGCHLD(/SIGCLD) (Child process terminated)";
    case SIGTTIN:
      return "SIGTTIN (Backgrounded read from terminal)";
    case SIGTTOU:
      return "SIGTTOU (Backgrounded write to terminal)";
    case SIGPOLL:
      return "SIGPOLL(/SIGIO) (Pollable event)";
    case SIGXCPU:
      return "SIGXCPU (CPU time limit exceeded)";
    case SIGXFSZ:
      return "SIGXFSZ (File size limit exceeded)";
    case SIGVTALRM:
      return "SIGVTALRM (Virtual alarm tocked)";
    case SIGPROF:
      return "SIGPROF (Profiling timer expired)";
    case SIGUSR1:
      return "SIGUSR1";
    case SIGUSR2:
      return "SIGUSR2";
    case SIGWINCH:
      return "SIGWINCH (Window size changed)";
    case SIGSTKFLT:
      return "SIGSTKFLT (Stack fault)";
    case SIGPWR:
      return "SIGPWR (Power failure)";
  }
#else  /* !MONOMUX_NON_ESSENTIAL_LOGS */
  (void)S;
#endif /* MONOMUX_NON_ESSENTIAL_LOGS */

  return "<unknown signal>";
}

void SignalTraits<PlatformTag::UNIX>::signalDispatch(Signal SigNum,
                                                     ::siginfo_t* Info,
                                                     void* /*Context*/)
{
  if (static_cast<std::size_t>(SigNum) > PlatformSpecificSignalTraits::Count)
  {
    LOG(error) << "Unhandleable too large signal number received";
    return;
  }

  SignalHandling* volatile Context = &SignalHandling::get();
  if (!Context)
    return;

  SignalHandling::CallbackArray* volatile CbArr =
    &Context->Callbacks.at(SigNum);
  if (!CbArr)
    return;
  for (std::size_t I = 0; I < SignalHandling::CallbackCount; ++I)
  {
    SignalHandling::Callback* volatile Cb = &CbArr->at(I);
    if (!Cb || !*Cb)
      return;

    (*Cb)(SigNum, Info, Context);
  }
}

void SignalTraits<PlatformTag::UNIX>::setSignalHandled(SignalHandling::Signal S)
{
  POD<struct ::sigaction> SigAct;
  SigAct->sa_flags = SA_SIGINFO;
  SigAct->sa_sigaction = signalDispatch;

  CheckedErrnoThrow([S, &SigAct] { return ::sigaction(S, &SigAct, nullptr); },
                    "sigaction(" + std::to_string(S) + ")",
                    -1);

  MONOMUX_TRACE_LOG(LOG(trace)
                    << SignalHandling::signalName(S) << " set to handle");
}

void SignalTraits<PlatformTag::UNIX>::setSignalDefault(SignalHandling::Signal S)
{
  POD<struct ::sigaction> SigAct;
  SigAct->sa_handler = SIG_DFL;

  CheckedErrnoThrow([S, &SigAct] { return ::sigaction(S, &SigAct, nullptr); },
                    "sigaction(" + std::to_string(S) + ", SIG_DFL)",
                    -1);

  MONOMUX_TRACE_LOG(LOG(trace)
                    << SignalHandling::signalName(S) << " set to default");
}

void SignalTraits<PlatformTag::UNIX>::setSignalIgnored(SignalHandling::Signal S)
{
  POD<struct ::sigaction> SigAct;
  SigAct->sa_handler = SIG_IGN;

  CheckedErrnoThrow([S, &SigAct] { return ::sigaction(S, &SigAct, nullptr); },
                    "sigaction(" + std::to_string(S) + ", SIG_IGN)",
                    -1);

  MONOMUX_TRACE_LOG(LOG(trace)
                    << SignalHandling::signalName(S) << " set to ignore");
}

} // namespace monomux::system

#undef LOG
