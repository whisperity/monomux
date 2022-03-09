if (NOT UNIX)
  message(FATAL_ERROR "This project supports only POSIX Unix/Linux platforms!")
endif()

add_library(monomuxSystem STATIC
  Environment.cpp
  epoll.cpp
  fd.cpp
  Process.cpp
  Pty.cpp
  Socket.cpp
  unreachable.cpp
  )
