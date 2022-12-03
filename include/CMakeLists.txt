add_subdirectory(core)
add_subdirectory(frontend)

if (MONOMUX_PLATFORM_UNIX)
  add_subdirectory(unix)
endif()
