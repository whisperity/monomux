list(APPEND libmonomuxCore_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/MessageBase.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Message.hpp
  )
set(libmonomuxCore_HEADERS "${libmonomuxCore_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Message.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)
