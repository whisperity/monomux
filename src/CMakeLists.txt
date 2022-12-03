# Create some variables to store files needed for distributing Monomux's core as
# a reusable library.
set(libmonomuxCore_SOURCES "")
add_subdirectory(core)

# Create some variables to store filenames needed to package the
# platform-specific tooling.
set(libmonomuxPlatform_SOURCES "")
if (MONOMUX_PLATFORM_UNIX)
  add_subdirectory(unix)
endif()

# Create some variables to store filenames needed to package the implementation
# of Monomux's internals.
set(libmonomuxImplementation_SOURCES "")
add_subdirectory(implementation)

# Create some variables to store filenames needed to package the reference
# implementation of Monomux's client-facing program.
set(libmonomuxFrontend_SOURCES "")
add_subdirectory(frontend)

# Create variables to store the filenames that are only needed for the
# user-facing entry point.
set(binmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  # monomuxCore is the library that contains the freely embeddable subset of
  # Monomux that exposes a reusable API. This should be safe to embed because
  # everything done and created by the library is owned by the library
  # instances.
  add_library(monomuxCore ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxCore_SOURCES}
    )
  add_dependencies(monomuxCore
    monomux_generate_version_h)

  # monomuxPlatform is the platform-specific library that implements OS
  # primitives against the interface specified by monomuxCore.
  add_library(monomux${MONOMUX_PLATFORM}Platform ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxPlatform_SOURCES}
    )
  target_link_libraries(monomux${MONOMUX_PLATFORM}Platform PUBLIC
    monomuxCore
    util
    dl
    )

  # monomuxImplementation contains the implementation of a capable Server and
  # Client built on top of monomuxCore.
  add_library(monomuxImplementation ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxImplementation_SOURCES}
    )
  target_link_libraries(monomuxImplementation PUBLIC
    monomuxCore
    monomux${MONOMUX_PLATFORM}Platform
    )

  # monomuxFrontend contains additional tools that might not be completely safe
  # to embed in another application, because direct control of application
  # state (signals, terminal, etc.) is done in this library.
  add_library(monomuxFrontend ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxFrontend_SOURCES}
    )
  target_link_libraries(monomuxFrontend PUBLIC
    monomuxCore
    monomux${MONOMUX_PLATFORM}Platform
    monomuxImplementation
    )

  # The monomux binary adds the user-facing command-line parset to the mix,
  # creating a proper executable.
  add_executable(monomux
    ${binmonomux_SOURCES}
    )
  target_link_libraries(monomux PUBLIC
    monomuxCore
    monomux${MONOMUX_PLATFORM}Platform
    monomuxImplementation
    monomuxFrontend
    )
else()
  # In Unity build, we'll create a single output executable comprised of all
  # the source files that would have been part of separate libraries.
  add_executable(monomux
    ${libmonomuxCore_SOURCES}
    ${libmonomuxPlatform_SOURCES}
    ${libmonomuxImplementation_SOURCES}
    ${libmonomuxFrontend_SOURCES}
    ${binmonomux_SOURCES}
    )
  add_dependencies(monomux
    monomux_generate_version_h)
  target_link_libraries(monomux PUBLIC
    dl
    util
    )
endif()

set_target_properties(monomux PROPERTIES
  # Put resulting binary to <Build>/, not <Build>/src/...
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  install(TARGETS monomuxCore
    COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
    )
  install(TARGETS monomux${MONOMUX_PLATFORM}Platform
    COMPONENT "${MONOMUX_PLATFORM_LIBRARY_DEV_NAME}"
    )
  install(TARGETS monomuxFrontend
    COMPONENT "${MONOMUX_FRONTEND_LIBRARY_DEV_NAME}"
    )
endif()

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "SHARED")
  # If we are using static libs or unity build, the main target only needs the
  # binary that got the static library linked in.
  install(TARGETS monomux
    COMPONENT "${MONOMUX_NAME}"
    )
else()
  # If we are using SHARED libs, the main install target needs the shared libs
  # too!
  install(TARGETS
      monomux
      monomuxCore
      monomux${MONOMUX_PLATFORM}Platform
      monomuxImplementation
      monomuxFrontend
    COMPONENT "${MONOMUX_NAME}")
endif()
