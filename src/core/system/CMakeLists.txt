list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Backtrace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/BufferedChannel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Handle.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Pipe.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SignalHandling.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Socket.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)
