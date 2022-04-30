list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Client.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ControlClient.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Dispatch.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)

list(APPEND libmonomuxImplementation_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Terminal.cpp
  )
set(libmonomuxImplementation_SOURCES "${libmonomuxImplementation_SOURCES}" PARENT_SCOPE)
