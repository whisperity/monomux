# SPDX-License-Identifier: LGPL-3.0-only
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(system)

set(libmonomuxPlatform_SOURCES "${libmonomuxPlatform_SOURCES}" PARENT_SCOPE)

set(libmonomuxPlatform_LIBS "${libmonomuxPlatform_LIBS}" PARENT_SCOPE)

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  if (NOT ${MONOMUX_PLATFORM} STREQUAL "Unix")
    message(FATAL_ERROR
      "Attempted to compile Unix platform module but the current platform is: "
      "${MONOMUX_PLATFORM}"
      )
  endif()

  # monomuxPlatform is the platform-specific library that implements OS
  # primitives against the interface specified by monomuxCore.
  add_library(monomux${MONOMUX_PLATFORM}Platform ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxPlatform_SOURCES}
    )
  if (libmonomuxPlatform_DEPENDS)
    add_dependencies(monomux${MONOMUX_PLATFORM}Platform
      ${libmonomuxPlatform_DEPENDS}
      )
  endif()
  if (libmonomuxPlatform_LIBS)
    target_link_libraries(monomux${MONOMUX_PLATFORM}Platform PUBLIC
      ${libmonomuxPlatform_LIBS}
      )
  endif()
  target_link_libraries(monomux${MONOMUX_PLATFORM}Platform PUBLIC
    monomuxCore
    )
endif()
