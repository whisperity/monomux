# SPDX-License-Identifier: LGPL-3.0-only
if (NOT ${MONOMUX_PLATFORM} STREQUAL "Unix")
  message(FATAL_ERROR
    "Attempted to compile Unix platform module but the current platform is: "
    "${MONOMUX_PLATFORM}"
    )
endif()

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(system)

set(libmonomuxPlatform_SOURCES "${libmonomuxPlatform_SOURCES}" PARENT_SCOPE)

set(libmonomuxPlatform_LIBS "${libmonomuxPlatform_LIBS}" PARENT_SCOPE)
