set(MONOMUX_CORE_LIBRARY_NAME "MonomuxCoreLibrary")
set(MONOMUX_CORE_LIBRARY_DEV_NAME "MonomuxCoreLibraryDevelopoment")


set(MONOMUX_NON_ESSENTIAL_LOGS ON CACHE BOOL
  "If set, the built binary will contain some additional log outputs that are needed for verbose debugging of the project. Turn off to cut down further on the binary size for production."
  )

configure_file(cmake/Config.in.h include/monomux/Config.h)
install(FILES "${CMAKE_BINARY_DIR}/include/monomux/Config.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/monomux"
  COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
  )
