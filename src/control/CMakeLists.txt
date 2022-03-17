list(APPEND libmonomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageBase.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Message.hpp
  )
set(libmonomux_HEADERS "${libmonomux_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Message.cpp
  )
set(libmonomux_SOURCES "${libmonomux_SOURCES}" PARENT_SCOPE)
