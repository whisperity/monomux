cmake_minimum_required(VERSION 3.10)
project(monomux LANGUAGES CXX)

if(NOT UNIX)
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

if( (CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") )
  check_add_compile_option(-Wall)
  check_add_compile_option(-Wextra)
endif()

add_executable(monomux
  main.cpp

  client/Client.Main.cpp
  client/ServerConnection.cpp

  control/Message.cpp

  server/Server.Main.cpp
  server/Server.cpp

  system/Process.cpp
  system/Pty.cpp
  system/Socket.cpp
  system/unreachable.cpp
  )
target_include_directories(monomux PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(monomux PUBLIC
  util)
