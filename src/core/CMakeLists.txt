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
