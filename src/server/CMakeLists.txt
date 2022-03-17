list(APPEND libmonomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientData.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.ipp
  ${CMAKE_CURRENT_SOURCE_DIR}/Server.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionData.hpp
  )
set(libmonomux_HEADERS "${libmonomux_HEADERS}" PARENT_SCOPE)
list(APPEND libmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientData.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SessionData.cpp
  )
set(libmonomux_SOURCES "${libmonomux_SOURCES}" PARENT_SCOPE)

list(APPEND monomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.hpp
  )
set(monomux_HEADERS "${monomux_HEADERS}" PARENT_SCOPE)
list(APPEND monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  )
set(monomux_SOURCES "${monomux_SOURCES}" PARENT_SCOPE)
