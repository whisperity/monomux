list(APPEND libmonomuxPlatform_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/EPoll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixBacktrace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixBacktraceDetail.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixPlatform.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixPipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixProcess.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixPty.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixSignalHandling.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixSocket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/UnixDomainSocket.cpp
  )
set(libmonomuxPlatform_SOURCES "${libmonomuxPlatform_SOURCES}" PARENT_SCOPE)
