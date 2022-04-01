cmake_minimum_required(VERSION 3.15.0)

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake"
  )
include(Version)

project(monomux
  LANGUAGES CXX
  VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}
  DESCRIPTION "MonoMux: Monophone Terminal Multiplexer"
  HOMEPAGE_URL "http://github.com/whisperity/MonoMux")

if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
endif()

set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "Monomux")

# Support adding compiler diagnostic flags dynamically, based on whether the
# compiler supports them.
include(CheckCXXCompilerFlag)
function(check_add_compile_option OPT)
  # Create an output variable for check_cxx_compiler_flag().
  string(REGEX REPLACE "^[-/]" "" var ${OPT})
  string(REGEX REPLACE ":" "" var ${var})
  check_cxx_compiler_flag(${OPT} ${var})
  if (${var})
    add_compile_options(${OPT})
  else()
    message(STATUS "Skip unsupported ${OPT} with the current compiler.")
  endif()
endfunction()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ( (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") )
  check_add_compile_option(-Wall)
  check_add_compile_option(-Wextra)
endif()

# TODO: Add -UNDEBUG so #ifndef NDEBUG and asserts are there for RelWithDebInfo.

include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/include"
  )

add_subdirectory(src)

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
    DESTINATION bin
    )
  install(FILES
      "${CMAKE_SOURCE_DIR}/README.md"
      "${CMAKE_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/monomux"
    )
endif()

# This has to be included *after* setting the variables up for some reason...
include(CPack)

cpack_add_component(monomux
  DISPLAY_NAME "MonoMux"
  REQUIRED)

add_subdirectory(test)
