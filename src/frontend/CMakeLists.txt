# SPDX-License-Identifier: GPL-3.0-only
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(client)
add_subdirectory(server)

list(APPEND libmonomuxFrontend_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CommandLine.cpp
  )
set(libmonomuxFrontend_SOURCES "${libmonomuxFrontend_SOURCES}" PARENT_SCOPE)
