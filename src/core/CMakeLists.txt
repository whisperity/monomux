include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(message)
add_subdirectory(system)

list(APPEND libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unreachable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Version.cpp
  )
set(libmonomuxCore_SOURCES "${libmonomuxCore_SOURCES}" PARENT_SCOPE)
