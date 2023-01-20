# SPDX-License-Identifier: GPL-3.0-only
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(client)
add_subdirectory(server)

# list(APPEND libmonomuxFrontend_SOURCES
#   )
set(libmonomuxFrontend_SOURCES "${libmonomuxFrontend_SOURCES}" PARENT_SCOPE)

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  # monomuxFrontend contains additional tools that might not be completely safe
  # to embed in another application, because direct control of application
  # state (signals, terminal, etc.) is done in this library.
  add_library(monomuxFrontend ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxFrontend_SOURCES}
    )
  if (libmonomuxFrontend_DEPENDS)
    add_dependencies(monomuxFrontend
      ${libmonomuxFrontend_DEPENDS}
      )
  endif()
  if (libmonomuxFrontend_LIBS)
    target_link_libraries(monomuxFrontend PUBLIC
      ${libmonomuxFrontend_LIBS}
      )
  endif()
  target_link_libraries(monomuxFrontend PUBLIC
    monomuxImplementation
    )
endif()
