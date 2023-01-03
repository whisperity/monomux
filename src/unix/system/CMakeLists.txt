# SPDX-License-Identifier: LGPL-3.0-only
list(APPEND libmonomuxPlatform_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/EPoll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Platform.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixBacktrace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixBacktraceDetail.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixDomainSocket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixPipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixProcess.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixPty.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixSignalHandling.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixSocket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixTerminal.cpp
  )
set(libmonomuxPlatform_SOURCES "${libmonomuxPlatform_SOURCES}" PARENT_SCOPE)
