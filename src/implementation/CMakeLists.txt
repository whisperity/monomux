# SPDX-License-Identifier: LGPL-3.0-only
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_dto_target(monomux_generate_messaging Monomux.dto)

add_subdirectory(client)
add_subdirectory(server)

list(APPEND libmonomuxImplementation_SOURCES
  ${monomux_generate_messaging_OUTPUTS}
  )
set(libmonomuxImplementation_SOURCES "${libmonomuxImplementation_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxImplementation_DEPENDS
  monomux_generate_messaging
  )
set(libmonomuxImplementation_DEPENDS "${libmonomuxImplementation_DEPENDS}" PARENT_SCOPE)

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  # monomuxImplementation contains the implementation of a capable Server and
  # Client built on top of monomuxCore.
  add_library(monomuxImplementation ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxImplementation_SOURCES}
    )
  if (libmonomuxImplementation_DEPENDS)
    add_dependencies(monomuxImplementation
      ${libmonomuxImplementation_DEPENDS}
      )
  endif()
  if (libmonomuxImplementation_LIBS)
    target_link_libraries(monomuxImplementation PUBLIC
      ${libmonomuxImplementation_LIBS}
      )
  endif()
  target_link_libraries(monomuxImplementation PUBLIC
    monomuxCore
    monomux${MONOMUX_PLATFORM}Platform
    )
endif()
