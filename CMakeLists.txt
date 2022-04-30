cmake_minimum_required(VERSION 3.15.0)

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake"
  )
include(MonomuxVersion)

project(monomux
  LANGUAGES CXX
  VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}
  DESCRIPTION "MonoMux: Monophone Terminal Multiplexer"
  HOMEPAGE_URL "http://github.com/whisperity/MonoMux")

if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
endif()

include(GNUInstallDirs)
include(MonomuxConfig)

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

message(STATUS "- * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - ")
message(STATUS "        MonoMux (v${MONOMUX_VERSION_STRING})")
message(STATUS "------------------------------------------------------------------------------")
message(STATUS "Build type:                                         ${CMAKE_BUILD_TYPE}")
message(STATUS "System:                                             ${CMAKE_SYSTEM_PROCESSOR} (${CMAKE_SYSTEM_NAME})")
message(STATUS "C++ standard:                                       C++${CMAKE_CXX_STANDARD}")
message(STATUS "Library type:                                       ${MONOMUX_LIBRARY_TYPE}")
message(STATUS "Non-essential log output:                           ${MONOMUX_NON_ESSENTIAL_LOGS}")
message(STATUS "- * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - ")

# TODO: Add -UNDEBUG so #ifndef NDEBUG and asserts are there for RelWithDebInfo.

include_directories(
  "${CMAKE_SOURCE_DIR}/include/core"
  "${CMAKE_SOURCE_DIR}/include/implementation"
  "${CMAKE_BINARY_DIR}/include"
  )

add_subdirectory(include)
add_subdirectory(src)

include(MonomuxCPack)

add_subdirectory(test)
