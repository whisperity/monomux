set(CPACK_PACKAGE_NAME "MonoMux")
set(CPACK_PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}")
set(CPACK_PACKAGE_VENDOR "Whisperity")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MonoMux: Monophone Terminal Multiplexer - Less intrusive than tmux, smarter than screen")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CONTACT "Whisperity <whisperity-packages@protonmail.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_EXECUTABLES monomux "MonoMux")
set(CPACK_CREATE_DESKTOP_LINKS monomux)

set(CPACK_BINARY_TZ OFF)
set(CPACK_BINARY_STGZ OFF)
set(CPACK_ARCHIVE_PACKAGE_DEBUG ON)
# Create one TGZ for each component.
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

if (UNIX)
  set(CPACK_BINARY_DEB ON)
  # set(CPACK_DEBIAN_PACKAGE_DEBUG ON)

  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.25), libstdc++6 (>= 5.2), libgcc1 (>= 1:8)")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://github.com/whisperity/MonoMux")

  # Create one Debian package for each (top level) component.
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  set(CPACK_DEBIAN_MONOMUXUSERGROUP_PACKAGE_NAME "monomux")

  set(CPACK_DEBIAN_MONOMUXDEVELOPERGROUP_PACKAGE_NAME "libmonomux-dev")
  set(CPACK_COMPONENT_MONOMUXDEVELOPERGROUP_DESCRIPTION "Contains the library code that allows reusing MonoMux inside another project. This package only contains both the core library and the user-facing reference implementation details.")
  set(CPACK_DEBIAN_MONOMUXDEVELOPERGROUP_DESCRIPTION "${CPACK_COMPONENT_MONOMUXDEVELOPERGROUP_DESCRIPTION}")

  set(CPACK_DEBIAN_MONOMUXUSERGROUP_PACKAGE_SECTION "admin")
  set(CPACK_DEBIAN_MONOMUXDEVELOPERGROUP_PACKAGE_SECTION "devel")

  set(CPACK_DEBIAN_MONOMUXUSERGROUP_DEBUGINFO_PACKAGE ON)

  # Install some additional resources into the Unix package.
  set(EXTRA_FILES_TO_INSTALL
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/LICENSE"
    )
  install(FILES ${EXTRA_FILES_TO_INSTALL}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/monomux"
    COMPONENT "${MONOMUX_NAME}"
    )
  install(FILES ${EXTRA_FILES_TO_INSTALL}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/libmonomux-dev"
    COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
    )
  install(FILES ${EXTRA_FILES_TO_INSTALL}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/libmonomux-dev"
    COMPONENT "${MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME}"
    )
endif()

# Run CPack. It needs the variables above to be set up correctly before include.
include(CPack)

# But the cpack_add_component() function comes from the CPack.cmake, so this
# has to come after.

cpack_add_component_group(MonomuxUserGroup
  DISPLAY_NAME "MonoMux application"
  DESCRIPTION "Contains the MonoMux client for everyday use."
  BOLD_TITLE
  )
cpack_add_install_type(MonomuxUserType
  DISPLAY_NAME "Client"
  )

cpack_add_component_group(MonomuxDeveloperGroup
  DISPLAY_NAME "MonoMux reusable library"
  DESCRIPTION "Contains the MonoMux reusable ${MONOMUX_LIBRARY_TYPE} libraries. This is intended for developers only."
  )
cpack_add_install_type(MonomuxDeveloperType
  DISPLAY_NAME "Library"
  )

cpack_add_component(${MONOMUX_NAME}
  DISPLAY_NAME "MonoMux"
  REQUIRED
  GROUP MonomuxUserGroup
  INSTALL_TYPES
    MonomuxUserType
    MonomuxDeveloperType
  )

cpack_add_component(${MONOMUX_CORE_LIBRARY_DEV_NAME}
  DISPLAY_NAME "MonoMux Core - Development package"
  DESCRIPTION "Contains the library code that allows reusing MonoMux inside another project. This package only contains the library, without the user-facing reference implementation."
  GROUP MonomuxDeveloperGroup
  INSTALL_TYPES
    MonomuxDeveloperType
  DISABLED
  )

cpack_add_component(${MONOMUX_IMPLEMENTATION_LIBRARY_DEV_NAME}
  DISPLAY_NAME "MonoMux Implementation - Development package"
  DESCRIPTION "Contains the library code that allows reusing MonoMux inside another project. This package only contains the user-facing reference implementation details."
  DEPENDS
    "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
  GROUP MonomuxDeveloperGroup
  INSTALL_TYPES
    MonomuxDeveloperType
  DISABLED
  )
