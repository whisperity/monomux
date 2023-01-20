# SPDX-License-Identifier: LGPL-3.0-only
set(RESOURCE_COMPILER_ROOT "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL
  "Source directory for the 'resource_compiler' tool." FORCE)

add_executable(resource_compiler
  ResourceCompiler.cpp

  # This file is not really needed to run the executable as it is just what
  # becomes part of the generated code, but it would be beneficial to see
  # compiler errors within this file during the build anyway.
  EmbeddedResources.cpp
  )
if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  target_link_libraries(resource_compiler PUBLIC
    monomuxCore
    )
else()
  target_link_libraries(resource_compiler PUBLIC
    monomuxCoreStatic
    )
endif()

# Resets the cache of the resource embedding system.
function(reset_resources)
  set(EMBEDDED_RESOURCES "" CACHE INTERNAL
        "List of files to be embedded as resources in the resulting build"
      )
endfunction()

# Adds a directive of FILE to be compiled and embedded under the resource
# identifier NAME to the build system.
#
# Note that this function does **not** execute the compilation of the resources,
# only prepares the system.
function(embed_resource NAME FILE)
  if (NAME MATCHES "=")
    message(SEND_ERROR "Resource identifier ${NAME} must not contain '='!")
  endif()

  if (NOT EMBEDDED_RESOURCES)
    set(EMBEDDED_RESOURCES "")
  endif()

  list(FIND EMBEDDED_RESOURCES "${NAME}=${FILE}" NAME_FOUND)
  if (NAME_FOUND GREATER -1)
    return()
  endif()

  list(APPEND EMBEDDED_RESOURCES
    "${NAME}=${FILE}"
    )
  set(EMBEDDED_RESOURCES "${EMBEDDED_RESOURCES}" CACHE INTERNAL
        "List of files to be embedded as resources in the resulting build"
      )
endfunction()

# Generate a compiled version of the already set up embedded resources
# (see embed_resource) that can be used in the C/C++ project.
# The main namespace (in C++ mode) for the resource embedding will be set to
# PROJECT_NAME.
# The output consists of a generated header file in HEADER_DIR, which exposes
# the functions that can be used to obtain a resource, and a generated SOURCE
# file which must be compiled to ship the data.
# If RESOURCE_LIBRARY is a non-empty string, a STATIC LIBRARY target, named
# RESOURCE_LIBRARY, will be created which will contain the compiled resources
# and infrastructure implementation for easy consumption.
function(generate_resources RESOURCE_LIBRARY PROJECT_NAME HEADER_DIR SOURCE)
  set(EMBEDDED_RESOURCES_PROJECT_NAMESPACE "${PROJECT_NAME}")

  set(OUT_CONFIG_H "${HEADER_DIR}/EmbeddedResources_Config.h")
  configure_file(
    "${RESOURCE_COMPILER_ROOT}/Config.in.h"
    "${OUT_CONFIG_H}"
    )

  set(HEADER "${HEADER_DIR}/EmbeddedResources.hpp")

  set(EMBEDDED_RESOURCES_INPUTS "")
  foreach(INPUT IN LISTS EMBEDDED_RESOURCES)
    string(FIND ${INPUT} "=" INPUT_SEPARATOR_LOC)
    if (INPUT_SEPARATOR_LOC EQUAL -1)
      message(FATAL_ERROR "Invalid badly separated value found in EMBEDDED_RESOURCES!")
    endif()
    math(EXPR INPUT_SEPARATOR_LOC "${INPUT_SEPARATOR_LOC} + 1")

    string(SUBSTRING ${INPUT} ${INPUT_SEPARATOR_LOC} -1 INPUT_VALUE)
    list(APPEND EMBEDDED_RESOURCES_INPUTS ${INPUT_VALUE})
  endforeach()

  add_custom_command(
    OUTPUT
      "${HEADER}"
      "${SOURCE}"
    COMMENT "Generating resources for ${PROJECT_NAME}..."
    DEPENDS
      resource_compiler
      ${RESOURCE_COMPILER_ROOT}/Config.in.h
      ${RESOURCE_COMPILER_ROOT}/EmbeddedResources.hpp
      ${EMBEDDED_RESOURCES_INPUTS}
    COMMAND
      resource_compiler
        "${RESOURCE_COMPILER_ROOT}"
        "${HEADER}"
        "${SOURCE}"
        ${EMBEDDED_RESOURCES}
    )
  set_source_files_properties(
    "${SOURCE}"
    PROPERTIES
      GENERATED TRUE
    )
  set_source_files_properties(
    "${OUT_CONFIG_H}"
    "${HEADER}"
    PROPERTIES
      GENERATED TRUE
      HEADER_ONLY_FILE TRUE
    )

  add_custom_target("${PROJECT_NAME}_generate_resources"
    DEPENDS
      "${SOURCE}"
      "${HEADER}"
      "${OUT_CONFIG_H}"
    )
  set_property(TARGET "${PROJECT_NAME}_generate_resources"
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES
      "${OUT_CONFIG_H}"
      "${HEADER}"
      "${SOURCE}"
    )

  if (NOT RESOURCE_LIBRARY STREQUAL "")
    # file(TOUCH "${SOURCE}")
    add_library(${RESOURCE_LIBRARY} STATIC
      "${OUT_CONFIG_H}"
      "${HEADER}"
      "${SOURCE}"
      )
    add_dependencies(${RESOURCE_LIBRARY} "${PROJECT_NAME}_generate_resources")
  endif()
endfunction()
