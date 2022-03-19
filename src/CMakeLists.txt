include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# Create some variables to store files needed for distributing MonomuxCore as an
# installable library.
set(monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )
set(monomux_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/ExitCode.hpp
  )
set(libmonomux_SOURCES)
set(libmonomux_HEADERS)

add_subdirectory(adt)
add_subdirectory(client)
add_subdirectory(control)
add_subdirectory(server)
add_subdirectory(system)

add_library(monomuxCore STATIC
  ${libmonomux_SOURCES}
  ${libmonomux_HEADERS}
  )

add_executable(monomux
  ${monomux_SOURCES}
  ${monomux_HEADERS}
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
