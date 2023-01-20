# SPDX-License-Identifier: LGPL-3.0-only
set(MONOMUX_CORE_LIBRARY_NAME "MonomuxCoreLibrary")
set(MONOMUX_CORE_LIBRARY_DEV_NAME "MonomuxCoreLibraryDevelopoment")

set(MONOMUX_IMPLEMENTATION_LIBRARY_NAME "MonomuxImplementationLibrary")
set(MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME "MonomuxImplementationLibraryDevelopment")

set(MONOMUX_FRONTEND_LIBRARY_NAME "MonomuxFrontendLibrary")
set(MONOMUX_FRONTEND_LIBRARY_DEV_NAME "MonomuxFrontendLibraryDevelopment")

set(MONOMUX_NAME "Monomux")
set(MONOMUX_DEV_NAME "MonomuxDevelopment")


set(MONOMUX_BUILD_SHARED_LIBS OFF CACHE BOOL
  "If set, the built binaries will be composed of several shared library (.so, .dll) for reusability in other projects. Turn off to improve optimisations if Monomux is only used as the reference implementation tool (normally it is)."
  )
if (NOT MONOMUX_BUILD_SHARED_LIBS)
  set(MONOMUX_LIBRARY_TYPE "STATIC")
else()
  set(MONOMUX_LIBRARY_TYPE "SHARED")
endif()

set(MONOMUX_BUILD_UNITY OFF CACHE BOOL
  "If set, the built binary will be created from a SINGLE translation unit, which usually improves run-time performance a great deal, at the expence of significant drag on compiler performance.")
if (MONOMUX_BUILD_UNITY)
  if (MONOMUX_LIBRARY_TYPE STREQUAL "SHARED")
    message(WARNING "Unity build with shared libraries does not make much sense. Prioritising unity build.")
  endif()

  set(MONOMUX_LIBRARY_TYPE "UNITY")
  set(CMAKE_UNITY_BUILD ON)
  set(CMAKE_UNITY_BUILD_BATCH_SIZE 0)
else()
  unset(CMAKE_UNITY_BUILD CACHE)
  unset(CMAKE_UNITY_BUILD_BATCH_SIZE CACHE)
endif()

set(MONOMUX_EMBEDDING_LIBRARY_FEATURES ON CACHE BOOL
  "If set, the built binaries will contain additional data structures and code to allow for third-party downstream projects to reuse Monomux as a library. Turn off to build a smaller and more optimised binary that only contains the \"official\" feature set."
  )

set(MONOMUX_NON_ESSENTIAL_LOGS ON CACHE BOOL
  "If set, the built binaries will contain some additional log outputs that are needed for verbose debugging of the project. Turn off to cut down further on the binary size for production."
  )

configure_file("src/Config.in.h" "include/monomux/Config.h")
set_source_files_properties("include/monomux/Config.h"
  PROPERTIES
    GENERATED TRUE
    HEADER_FILE_ONLY TRUE
  )
install(FILES
    "${CMAKE_BINARY_DIR}/include/monomux/Config.h"
    "${CMAKE_BINARY_DIR}/include/monomux/Version.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/monomux"
  COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
  )
