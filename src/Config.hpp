/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <string>

#include "monomux/Config.h"

namespace monomux
{

/// \returns details about the configuration of the current Monomux build in a
/// human-readable format.
std::string getHumanReadableConfiguration();

} // namespace monomux
