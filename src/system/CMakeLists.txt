list(APPEND libmonomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CheckedPOSIX.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/CommunicationChannel.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/epoll.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pipe.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/POD.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Process.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pty.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Socket.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Time.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unreachable.hpp
  )
set(libmonomux_HEADERS "${libmonomux_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CommunicationChannel.cpp
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

list(APPEND monomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Signal.hpp
  )
set(monomux_HEADERS "${monomux_HEADERS}" PARENT_SCOPE)
list(APPEND monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Signal.cpp
  )
set(monomux_SOURCES "${monomux_SOURCES}" PARENT_SCOPE)
