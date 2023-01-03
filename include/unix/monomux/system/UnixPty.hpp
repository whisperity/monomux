/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <optional>

#include "monomux/system/Pty.hpp"

namespace monomux::system::unix
{

/// Responsible for wrapping a low-level psuedo terminal teletypewriter (PTTY)
/// interface. A pseudoterminal is an emulation of the ancient technology where
/// physical typewriter and printer machines were connected to computers.
///
/// \see pty(7)
/// \see openpty(3)
class Pty : public system::Pty
{
public:
  /// Creates a new PTY-pair.
  Pty();

  void setupParentSide() override;

  void setupChildrenSide() override;

  void setSize(unsigned short Rows, unsigned short Columns) override;
};

} // namespace monomux::system::unix
