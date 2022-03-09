cmake_minimum_required(VERSION 3.11.4)
project(monomux LANGUAGES CXX)

if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
endif()

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

add_subdirectory(src)

set(CPACK_PACKAGE_NAME "MonoMux")
set(CPACK_PACKAGE_VENDOR "http://github.com/whisperity/MonoMux")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MonoMux: Monophone Terminal Multiplexer - Less intrusive than tmux, smarter than screen")
set(CPACK_PACKAGE_CONTACT "Whisperity <whisperity-packages@protonmail.com")
set(CPACK_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")
set(CPACK_FILE_README "${CMAKE_SOURCE_DIR}/../README.md")
set(CPACK_PACKAGE_EXECUTABLES monomux "MonoMux")
set(CPACK_CREATE_DESKTOP_LINKS monomux)

if (UNIX)
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

# This has to be included *after* setting the variables up for some reason...
include(CPack)

add_subdirectory(test)
