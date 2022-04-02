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

if (UNIX)
  set(CPACK_BINARY_DEB ON)
  set(CPACK_DEB_COMPONENT_INSTALL ON)
  # Override to remove component name.
  set(CPACK_DEBIAN_MONOMUX_PACKAGE_NAME "monomux")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.25), libstdc++6 (>= 5.2), libgcc1 (>= 1:8)")
  set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://github.com/whisperity/MonoMux")
  set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)

  include(GNUInstallDirs)
  install(TARGETS monomux
    DESTINATION bin)
  install(FILES
      "${CMAKE_SOURCE_DIR}/README.md"
      "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/monomux"
    )
endif()

# Run CPack. It needs the variables above to be set up correctly before include.
include(CPack)

# But the cpack_add_component() function comes from the CPack.cmake, so this
# has to come after.
cpack_add_component(monomux
  DISPLAY_NAME "MonoMux"
  REQUIRED)
