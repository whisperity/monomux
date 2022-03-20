include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# Create some variables to store files needed for distributing Monomux as
# reusable libraries.
set(libmonomuxCore_SOURCES)
set(libmonomuxCore_HEADERS)
set(libmonomuxMain_SOURCES)
set(libmonomuxMain_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/ExitCode.hpp
  )
set(monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )
set(monomux_HEADERS)

add_subdirectory(adt)
add_subdirectory(client)
add_subdirectory(control)
add_subdirectory(server)
add_subdirectory(system)

# monomuxCore is the library that contains the freely embeddable subset of
# Monomux that expose a reusable API. This should be safe to embed because
# everything done and created by the library is owned by the library instances.
add_library(monomuxCore STATIC
  ${libmonomuxCore_SOURCES}
  ${libmonomuxCore_HEADERS}
  )

# monomuxMain contains the implementation of a capable Server and Client built
# on top of monomuxCore. This library contains additional tools that might not
# be completely safe to embed in another application, because direct control of
# application state (signals, terminal, etc.) is done here.
add_library(monomuxMain STATIC
  ${libmonomuxMain_SOURCES}
  ${libmonomuxMain_HEADERS}
  )
target_link_libraries(monomuxMain PUBLIC monomuxCore)

# The monomux binary adds the user-facing command-line parset to the mix,
# creating a proper executable.
add_executable(monomux
  ${monomux_SOURCES}
  ${monomux_HEADERS}
  )
target_link_libraries(monomux PUBLIC
  monomuxMain
  util
  )
set_target_properties(monomux PROPERTIES
  # Put resulting binary to <Build>/, not <Build>/src/...
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
