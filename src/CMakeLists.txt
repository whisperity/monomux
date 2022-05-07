include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# Create some variables to store files needed for distributing Monomux's core as
# a reusable library.
set(libmonomuxCore_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/unreachable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Version.cpp
  )

# Create some variables to store filenames needed to package the reference
# implementation of Monomux as a reusable library.
set(libmonomuxImplementation_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
  )

# Create variables to store the filenames that are only needed for the
# user-facing entry point.
set(monomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )

add_subdirectory(client)
add_subdirectory(control)
add_subdirectory(server)
add_subdirectory(system)

# monomuxCore is the library that contains the freely embeddable subset of
# Monomux that expose a reusable API. This should be safe to embed because
# everything done and created by the library is owned by the library instances.
add_library(monomuxCore ${MONOMUX_LIBRARY_TYPE}
  ${libmonomuxCore_SOURCES}
  )
add_dependencies(monomuxCore
  monomux_generate_version_h)
target_link_libraries(monomuxCore PUBLIC
  util
  )

# monomuxMain contains the implementation of a capable Server and Client built
# on top of monomuxCore. This library contains additional tools that might not
# be completely safe to embed in another application, because direct control of
# application state (signals, terminal, etc.) is done here.
add_library(monomuxImplementation ${MONOMUX_LIBRARY_TYPE}
  ${libmonomuxImplementation_SOURCES}
  )
target_link_libraries(monomuxImplementation PUBLIC
  monomuxCore
  dl
  util
  )

# The monomux binary adds the user-facing command-line parset to the mix,
# creating a proper executable.
add_executable(monomux
  ${monomux_SOURCES}
  )
target_link_libraries(monomux PUBLIC
  monomuxImplementation
  )
set_target_properties(monomux PROPERTIES
  # Put resulting binary to <Build>/, not <Build>/src/...
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS monomuxCore
  COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
  )
install(TARGETS monomuxImplementation
  COMPONENT "${MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME}"
  )

if (NOT MONOMUX_BUILD_SHARED_LIBS)
  # If we are using static libs, the main target only needs the binary that
  # got the static library linked in.
  install(TARGETS monomux
    COMPONENT "${MONOMUX_NAME}"
    )
else()
  # If we are using SHARED libs, the main install target needs the shared libs
  # too!
  install(TARGETS monomux monomuxCore monomuxImplementation
    COMPONENT "${MONOMUX_NAME}")
endif()
