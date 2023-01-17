# SPDX-License-Identifier: LGPL-3.0-only
add_subdirectory(core)
add_subdirectory(frontend)
add_subdirectory(implementation)

if (MONOMUX_PLATFORM_UNIX)
  add_subdirectory(unix)
endif()
