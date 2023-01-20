# SPDX-License-Identifier: GPL-3.0-only

# Create some variables to store files needed for distributing Monomux's core as
# a reusable library.
set(libmonomuxCore_SOURCES "")
set(libmonomuxCore_DEPENDS "")
set(libmonomuxCore_LIBS "")
add_subdirectory(core)

# Create some variables to store filenames needed to package the
# platform-specific tooling.
set(libmonomuxPlatform_SOURCES "")
set(libmonomuxPlatform_DEPENDS "")
set(libmonomuxPlatform_LIBS "")
if (MONOMUX_PLATFORM_UNIX)
  add_subdirectory(unix)
endif()

# Create some variables to store filenames needed to package the implementation
# of Monomux's internals.
set(libmonomuxImplementation_SOURCES "")
set(libmonomuxImplementation_DEPENDS "")
set(libmonomuxImplementation_LIBS "")
add_subdirectory(implementation)

# Create some variables to store filenames needed to package the reference
# implementation of Monomux's client-facing program.
set(libmonomuxFrontend_SOURCES "")
set(libmonomuxFrontend_DEPENDS "")
set(libmonomuxFrontend_LIBS "")
add_subdirectory(frontend)

# Create variables to store the filenames that are only needed for the
# user-facing entry point.
set(binmonomux_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  )
set(binmonomux_DEPENDS "")
set(binmonomux_LIBS "")

# Compile embeddable resources.
reset_resources()
generate_resources(monomuxResources
  "monomux"
  "${CMAKE_BINARY_DIR}/include/monomux/resources"
  "${CMAKE_CURRENT_BINARY_DIR}/Resources.cpp")

if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  # monomuxCore is the library that contains the freely embeddable subset of
  # Monomux that exposes a reusable API. This should be safe to embed because
  # everything done and created by the library is owned by the library
  # instances.
  add_library(monomuxCore ${MONOMUX_LIBRARY_TYPE}
    ${libmonomuxCore_SOURCES}
    ${libmonomuxPlatform_SOURCES}
    )
  if (libmonomuxCore_DEPENDS OR libmonomuxPlatform_DEPENDS)
    add_dependencies(monomuxCore
      ${libmonomuxCore_DEPENDS}
      ${libmonomuxPlatform_DEPENDS}
      )
  endif()
  if (libmonomuxCore_LIBS OR libmonomuxPlatform_LIBS)
    target_link_libraries(monomuxCore PUBLIC
      ${libmonomuxCore_LIBS}
      ${libmonomuxPlatform_LIBS}
      )
  endif()

  # The monomux binary adds the user-facing command-line parset to the mix,
  # creating a proper executable.
  add_executable(monomux
    ${binmonomux_SOURCES}
    )
  if (binmonomux_DEPENDS)
    add_dependencies(monomux
      ${binmonomux_DEPENDS}
      )
  endif()
  if (binmonomux_LIBS)
    target_link_libraries(monomux PUBLIC
      ${binmonomux_LIBS}
      )
  endif()
  target_link_libraries(monomux PUBLIC
    monomuxFrontend
    )
else()
  # Expose the true "core" libraries that implement system-specific stuff as a
  # reusable library **even** in Unity mode. (This is needed for the embedded
  # tooling.)
  add_library(monomuxCoreStatic STATIC
    ${libmonomuxCore_SOURCES}
    ${libmonomuxPlatform_SOURCES}
    )
  if (libmonomuxCore_DEPENDS OR libmonomuxPlatform_DEPENDS)
    add_dependencies(monomuxCoreStatic
      ${libmonomuxCore_DEPENDS}
      ${libmonomuxPlatform_DEPENDS}
      )
  endif()
  if (libmonomuxCore_LIBS OR libmonomuxPlatform_LIBS)
    target_link_libraries(monomuxCoreStatic PUBLIC
      ${libmonomuxCore_LIBS}
      ${libmonomuxPlatform_LIBS}
      )
  endif()

  # In Unity build, we'll create a single output executable comprised of all
  # the source files that would have been part of separate libraries. This is
  # generally expected to allow the compiler better optimise the resulting
  # binary. It is also expected that *most* users will consume the UNITY build.
  add_executable(monomux
    ${libmonomuxCore_SOURCES}
    ${libmonomuxPlatform_SOURCES}
    ${libmonomuxImplementation_SOURCES}
    ${libmonomuxFrontend_SOURCES}
    ${binmonomux_SOURCES}
    )
  if (libmonomuxCore_DEPENDS OR
      libmonomuxPlatform_DEPENDS OR
      libmonomuxImplementation_DEPENDS OR
      libmonomuxFrontend_DEPENDS OR
      binmonomux_DEPENDS)
    add_dependencies(monomux
      ${libmonomuxCore_DEPENDS}
      ${libmonomuxPlatform_DEPENDS}
      ${libmonomuxImplementation_DEPENDS}
      ${libmonomuxFrontend_DEPENDS}
      ${binmonomux_DEPENDS}
      )
  endif()
  if (libmonomuxCore_LIBS OR
      libmonomuxPlatform_LIBS OR
      libmonomuxImplementation_LIBS OR
      libmonomuxFrontend_LIBS OR
      binmonomux_LIBS)
    target_link_libraries(monomux PUBLIC
      ${libmonomuxCore_LIBS}
      ${libmonomuxPlatform_LIBS}
      ${libmonomuxImplementation_LIBS}
      ${libmonomuxFrontend_LIBS}
      ${binmonomux_LIBS}
      )
  endif()
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
  install(TARGETS monomuxImplementation
    COMPONENT "${MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME}"
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
      monomuxImplementation
      monomuxFrontend
    COMPONENT "${MONOMUX_NAME}")
endif()
