list(APPEND libmonomuxCore_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Client.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ControlClient.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.ipp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionData.hpp
  )
set(libmonomuxCore_HEADERS "${libmonomuxCore_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ControlClient.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxMain_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Terminal.hpp
  )
set(libmonomuxMain_HEADERS "${libmonomuxMain_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxMain_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Terminal.cpp
  )
set(libmonomuxMain_SOURCES "${libmonomuxMain_SOURCES}" PARENT_SCOPE)
