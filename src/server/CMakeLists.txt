list(APPEND libmonomuxCore_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientData.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.ipp
  ${CMAKE_CURRENT_SOURCE_DIR}/Server.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionData.hpp
  )
set(libmonomuxCore_HEADERS "${libmonomuxCore_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientData.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionData.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxImplementation_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.hpp
  )
set(libmonomuxImplementation_HEADERS "${libmonomuxImplementation_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxImplementation_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  )
set(libmonomuxImplementation_SOURCES "${libmonomuxImplementation_SOURCES}" PARENT_SCOPE)
