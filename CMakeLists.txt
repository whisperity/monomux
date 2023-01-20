# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.16.5)

# Just simply do not let the user do this, because this litters the project
# tree with weird contents that are not verbosely elaborated in a .gitignore...
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  file(REMOVE "CMakeCache.txt")
  file(REMOVE_RECURSE "CMakeFiles")

  message(FATAL_ERROR "Raw in-tree builds are not supported, please use a dedicated 'Build' directory!"
                      "\nDue to technical detail, 'CMakeCache.txt' and 'CMakeFiles/' will still be created as trash in your directory now, and you should remove them!")
endif()

list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake"
  )
include(MonomuxVersion)

project(monomux
  LANGUAGES CXX
  VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}
  DESCRIPTION "MonoMux: Monophone Terminal Multiplexer"
  HOMEPAGE_URL "http://github.com/whisperity/MonoMux"
  )

if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
else()
  set(MONOMUX_PLATFORM "Unix")
  set(MONOMUX_PLATFORM_UNIX ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
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
message(STATUS "Platform:                                           ${MONOMUX_PLATFORM}")
message(STATUS "System:                                             ${CMAKE_SYSTEM_PROCESSOR} (${CMAKE_SYSTEM_NAME})")
# message(STATUS "")
message(STATUS "Build type:                                         ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:                                       C++${CMAKE_CXX_STANDARD}")
# message(STATUS "")
message(STATUS "Library type:                                       ${MONOMUX_LIBRARY_TYPE}")
message(STATUS "Embedding support features:                         ${MONOMUX_EMBEDDING_LIBRARY_FEATURES}")
message(STATUS "Non-essential log output:                           ${MONOMUX_NON_ESSENTIAL_LOGS}")
message(STATUS "- * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - * - ")

# TODO: Add -UNDEBUG so #ifndef NDEBUG and asserts are there for RelWithDebInfo.

include_directories(
  "${CMAKE_SOURCE_DIR}/include/core"
  "${CMAKE_SOURCE_DIR}/include/frontend"
  "${CMAKE_SOURCE_DIR}/include/implementation"
  "${CMAKE_BINARY_DIR}/include"
  )

if (MONOMUX_PLATFORM_UNIX)
  include_directories(
    "${CMAKE_SOURCE_DIR}/include/unix"
    )
endif()

add_subdirectory(tools)

add_subdirectory(include)
add_subdirectory(src)

include(MonomuxDoxygen)
include(MonomuxCPack)

add_subdirectory(test)
