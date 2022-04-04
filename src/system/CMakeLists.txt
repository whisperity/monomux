list(APPEND libmonomuxCore_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.hpp
  )
set(libmonomuxCore_HEADERS "${libmonomuxCore_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CommunicationChannel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/EPoll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Process.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pty.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Socket.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxMain_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Signal.hpp
  )
set(libmonomuxMain_HEADERS "${libmonomuxMain_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxMain_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Signal.cpp
  )
set(libmonomuxMain_SOURCES "${libmonomuxMain_SOURCES}" PARENT_SCOPE)
