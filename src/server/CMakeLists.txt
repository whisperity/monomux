list(APPEND libmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/ClientData.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Server.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Session.cpp
  )
set(libmonomux_SOURCES "${libmonomux_SOURCES}" PARENT_SCOPE)

list(APPEND monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  )
set(monomux_SOURCES "${monomux_SOURCES}" PARENT_SCOPE)
