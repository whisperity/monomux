list(APPEND libmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/epoll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Process.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pty.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Socket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unreachable.cpp
  )
set(libmonomux_SOURCES "${libmonomux_SOURCES}" PARENT_SCOPE)
