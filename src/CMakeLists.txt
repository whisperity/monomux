include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(client)
add_subdirectory(control)
add_subdirectory(server)
add_subdirectory(system)

add_executable(monomux
  main.cpp
  )
target_link_libraries(monomux PUBLIC
  monomuxClient
  monomuxControl
  monomuxServer
  monomuxSystem

  util
  )
set_target_properties(monomux
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
