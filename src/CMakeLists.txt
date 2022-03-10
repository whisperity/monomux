include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )
set(libmonomux_SOURCES
  )

add_subdirectory(client)
add_subdirectory(control)
add_subdirectory(server)
add_subdirectory(system)

add_library(monomuxCore STATIC
  ${libmonomux_SOURCES}
  )

add_executable(monomux
  ${monomux_SOURCES}
  )
target_link_libraries(monomux PUBLIC
  monomuxCore
  util
  )
set_target_properties(monomux PROPERTIES
  # Put resulting binary to <Build>/, not <Build>/src/...
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
