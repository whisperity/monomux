list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/BufferedChannel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Event.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Process.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pty.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Socket.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fd.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxImplementation_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Crash.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Signal.cpp
  )
set(libmonomuxImplementation_SOURCES "${libmonomuxImplementation_SOURCES}" PARENT_SCOPE)
