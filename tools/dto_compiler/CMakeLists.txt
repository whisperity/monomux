# SPDX-License-Identifier: LGPL-3.0-only

add_executable(dto_compiler
  DTOCompiler.cpp
  Lex.cpp
  )
if (NOT MONOMUX_LIBRARY_TYPE STREQUAL "UNITY")
  target_link_libraries(dto_compiler PUBLIC
    monomuxCore
    monomux${MONOMUX_PLATFORM}Platform
    )
else()
  target_link_libraries(dto_compiler PUBLIC
    _monomuxCoreAndPlatform
    )
endif()

# Creates a custom target TARGET which compiles the INPUT file into a set of
# generated DTO sources.
#
# The files that will need to be compiled into a receiver library are made
# available under ${TARGET}_OUTPUTS.
function(add_dto_target TARGET INPUT)
  set("${TARGET}_OUTPUTS" "")
  list(APPEND "${TARGET}_OUTPUTS"
    ${CMAKE_CURRENT_BINARY_DIR}/Dummy.cpp
    )

  add_custom_command(
    OUTPUT
      ${${TARGET}_OUTPUTS}
    COMMENT "${TARGET}: Generating DTO from ${INPUT}..."
    MAIN_DEPENDENCY
      "${INPUT}"
    DEPENDS
      dto_compiler
    COMMAND
      dto_compiler ${INPUT} ${CMAKE_CURRENT_BINARY_DIR}
    )
  set_source_files_properties(${${TARGET}_OUTPUTS}
    PROPERTIES GENERATED TRUE
    )

  add_custom_target("${TARGET}"
    DEPENDS ${${TARGET}_OUTPUTS}
    )
  set("${TARGET}_OUTPUTS" "${${TARGET}_OUTPUTS}" PARENT_SCOPE)
endfunction()
