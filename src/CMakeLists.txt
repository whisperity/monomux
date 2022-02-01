cmake_minimum_required(VERSION 3.13)
project(monomux LANGUAGES CXX)

if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(monomux
  Process.cpp
  Pty.cpp
  Server.cpp
  ServerConnection.cpp
  Socket.cpp
  main.cpp)
target_link_libraries(monomux PUBLIC
  util)
