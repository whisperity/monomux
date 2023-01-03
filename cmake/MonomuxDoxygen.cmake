# SPDX-License-Identifier: LGPL-3.0-only
set(MONOMUX_BUILD_DOCS OFF CACHE BOOL
  "Whether to build documentation pages with Doxygen.")

if (NOT MONOMUX_BUILD_DOCS)
  return()
endif()

find_package(Doxygen
             REQUIRED dot
             )

if (NOT DOXYGEN_FOUND)
  message(SEND_ERROR "Documentation generation was enabled, but Doxygen is not installed. Set MONOMUX_BUILD_DOCS to OFF to disable this warning.")
  return()
endif()

set(MONOMUX_DOCS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src)
set(MONOMUX_DOCS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

configure_file(Doxyfile.in Doxyfile)
add_custom_target(docs
  COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
  COMMENT "Generating Doxygen documentation..."
  )
