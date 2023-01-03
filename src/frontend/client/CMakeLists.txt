# SPDX-License-Identifier: GPL-3.0-only
list(APPEND libmonomuxFrontend_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/App.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionManagement.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Terminal.cpp
  )
set(libmonomuxFrontend_SOURCES "${libmonomuxFrontend_SOURCES}" PARENT_SCOPE)
