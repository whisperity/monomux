# SPDX-License-Identifier: LGPL-3.0-only
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(message)
add_subdirectory(system)

list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unreachable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Version.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxCore_DEPENDS
  monomux_generate_version_h
  )
set(libmonomuxCore_DEPENDS "${libmonomuxCore_DEPENDS}" PARENT_SCOPE)

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  # monomuxCore is the library that contains the freely embeddable subset of
  # Monomux that exposes a reusable API. This should be safe to embed because
  # everything done and created by the library is owned by the library
  # instances.
  add_library(monomuxCore ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxCore_SOURCES}
    )
  if (libmonomuxCore_DEPENDS)
    add_dependencies(monomuxCore
      ${libmonomuxCore_DEPENDS}
      )
  endif()
  if (libmonomuxCore_LIBS)
    target_link_libraries(monomuxCore PUBLIC
      ${libmonomuxCore_LIBS}
      )
  endif()
endif()
