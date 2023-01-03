/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <unistd.h>

#include "monomux/CheckedErrno.hpp"
#include "monomux/system/Process.hpp"

namespace monomux::system::unix
{

/// Responsible for creating, executing, and handling processes on the
/// system.
class Process : public system::Process
{
public:
  bool reapIfDead() override;

  void wait() override;

  void signal(int Signal) override;

  /// \p fork(): Ask the kernel to create an exact duplicate of the current
  /// process. The specified callbacks \p ParentAction and \p ChildAction will
  /// be run in the parent and the child process, respectively.
  ///
  /// \note Execution continues normally after the callbacks retur return!
  template <typename ParentFn, typename ChildFn>
  static void fork(ParentFn ParentAction, ChildFn ChildAction)
  {
    auto ForkResult = CheckedErrnoThrow([] { return ::fork(); }, "fork()", -1);
    if (ForkResult == 0)
      ChildAction();
    else
      ParentAction();
  }
};

} // namespace monomux::system::unix
