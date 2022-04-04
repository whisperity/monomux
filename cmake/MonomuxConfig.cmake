set(MONOMUX_CORE_LIBRARY_NAME "MonomuxCoreLibrary")
set(MONOMUX_CORE_LIBRARY_DEV_NAME "MonomuxCoreLibraryDevelopoment")

set(MONOMUX_IMPLEMENTATION_LIBRARY_NAME "MonomuxLibrary")
set(MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME "MonomuxLibraryDevelopment")

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

set(MONOMUX_NON_ESSENTIAL_LOGS ON CACHE BOOL
  "If set, the built binary will contain some additional log outputs that are needed for verbose debugging of the project. Turn off to cut down further on the binary size for production."
  )

configure_file(cmake/Config.in.h include/monomux/Config.h)
install(FILES "${CMAKE_BINARY_DIR}/include/monomux/Config.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/monomux"
  COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
  )
