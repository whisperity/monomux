include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(system)

# list(APPEND libmonomuxPlatform_SOURCES
#   )
set(libmonomuxPlatform_SOURCES "${libmonomuxPlatform_SOURCES}" PARENT_SCOPE)
