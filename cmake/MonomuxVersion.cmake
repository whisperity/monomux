if (NOT DEFINED VERSION_HEADER_TEMPLATE OR NOT DEFINED VERSION_HEADER_RESULT)
  set(VERSION_HEADER_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/Version.in.h")
  set(VERSION_HEADER_RESULT "${CMAKE_BINARY_DIR}/include/monomux/Version.h")
endif()
if (NOT DEFINED VERSION_TXT_RESULT)
  set(VERSION_TXT_RESULT "${CMAKE_BINARY_DIR}/Version.txt")
endif()

# Emit the version information to the output.
function(print_version)
  if (NOT VERSION_HAS_EXTRAS)
    message(STATUS "[Version] Result version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}")
  else()
    if (VERSION_OFFSET)
      message(STATUS "[Version] Offset from tagged version: ${VERSION_OFFSET}")
    endif()
    if (VERSION_COMMIT)
      set(VERSION_HAS_EXTRAS ON)
      message(STATUS "[Version] On commit: ${VERSION_COMMIT}")
    endif()
    if (VERSION_DIRTY)
      message(STATUS "[Version] Uncommitted local changes found!")
    endif()

    message(STATUS "[Version] Result version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}-${VERSION_OFFSET}-${VERSION_COMMIT}${VERSION_DIRTY}")
  endif()
endfunction()

# Emit the configuration of the Version header.
function(write_version_header)
  configure_file("${VERSION_HEADER_TEMPLATE}" "${VERSION_HEADER_RESULT}")

  if (NOT VERSION_HAS_EXTRAS)
    file(WRITE "${VERSION_TXT_RESULT}"
      "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}")
  else()
    file(WRITE "${VERSION_TXT_RESULT}"
      "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}.${VERSION_TWEAK}-${VERSION_OFFSET}-${VERSION_COMMIT}${VERSION_DIRTY}")
  endif()
endfunction()

execute_process(COMMAND git
  OUTPUT_VARIABLE git
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if (NOT git)
  message(WARNING "[Version] No Git command found, unable to extract version data.")
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 0)
  set(VERSION_PATCH 0)
  set(VERSION_TWEAK 0)
  set(VERSION_HAS_EXTAS OFF)

  print_version()
  write_version()
  return()
endif()

execute_process(COMMAND git describe --always --tags --dirty=-dirty
  OUTPUT_VARIABLE git_describe_result
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_STRIP_TRAILING_WHITESPACE)

if (git_describe_result MATCHES "^v")
  string(REGEX MATCH
    "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9]*)-g([0-f]*))?(-dirty)?"
    _
    "${git_describe_result}")
  set(VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(VERSION_MINOR "${CMAKE_MATCH_2}")
  set(VERSION_PATCH "${CMAKE_MATCH_3}")
  set(VERSION_TWEAK 0)
  set(VERSION_HAS_EXTAS OFF)
  set(VERSION_OFFSET "${CMAKE_MATCH_5}")
  if (VERSION_OFFSET)
    set(VERSION_HAS_EXTRAS ON)
  else()
    set(VERSION_OFFSET 0)
  endif()
  set(VERSION_COMMIT "${CMAKE_MATCH_6}")
  if (VERSION_COMMIT)
    set(VERSION_HAS_EXTRAS ON)
  endif()
  if (CMAKE_MATCH_7 STREQUAL "-dirty")
    set(VERSION_HAS_EXTRAS ON)
    set(VERSION_DIRTY "-dirty")
  else()
    set(VERSION_DIRTY "")
  endif()
else()
  string(REGEX MATCH "^([0-f]*)(-dirty)?" _ "${git_describe_result}")
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 0)
  set(VERSION_PATCH 0)
  set(VERSION_TWEAK 0)
  set(VERSION_HAS_EXTAS OFF)
  set(VERSION_OFFSET 0)
  set(VERSION_COMMIT "${CMAKE_MATCH_1}")
  if (VERSION_COMMIT)
    set(VERSION_HAS_EXTRAS ON)
  endif()
  if (CMAKE_MATCH_2 STREQUAL "-dirty")
    set(VERSION_HAS_EXTRAS ON)
    set(VERSION_DIRTY "-dirty")
  else()
    set(VERSION_DIRTY "")
  endif()
endif()

if (VERSION_COMMIT)
  execute_process(COMMAND git rev-parse ${VERSION_COMMIT}
    OUTPUT_VARIABLE git_full_hash
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(VERSION_COMMIT "${git_full_hash}")
endif()

if (DEFINED ENV{GITHUB_RUN_NUMBER})
  set(VERSION_TWEAK "$ENV{GITHUB_RUN_NUMBER}")
  message(STATUS "[Version] Currently executing build $ENV{GITHUB_RUN_NUMBER}")
endif()

# Generate a trampoline script from a template which will be executed to write
# the version information into a generated header every time Git logs change.
if (NOT VERSIONING_FROM_TRAMPOLINE)
  print_version()

  set(VERSION_GENERATOR_SCRIPT "${CMAKE_BINARY_DIR}/cmake/_Version.cmake")

  configure_file("${CMAKE_SOURCE_DIR}/cmake/WriteVersion.in.cmake"
    "${VERSION_GENERATOR_SCRIPT}")

  execute_process(COMMAND git rev-parse --absolute-git-dir
    OUTPUT_VARIABLE git_dir_path
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  add_custom_command(OUTPUT "${VERSION_HEADER_RESULT}"
    COMMENT "Generating Version information"
    DEPENDS "${git_dir_path}/logs/HEAD"
      "${VERSION_GENERATOR_SCRIPT}"
      "${VERSION_HEADER_TEMPLATE}"
    COMMAND ${CMAKE_COMMAND}
      -P "${VERSION_GENERATOR_SCRIPT}")
  add_custom_target(generate_version_h DEPENDS "${VERSION_HEADER_RESULT}")

  set_source_files_properties("${VERSION_HEADER_RESULT}"
    PROPERTIES GENERATED TRUE
               HEADER_FILE_ONLY TRUE
    )

  install(FILES "${VERSION_HEADER_RESULT}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/monomux"
    COMPONENT "${MONOMUX_CORE_LIBRARY_DEV_NAME}"
    )

  unset(VERSION_GENERATOR_SCRIPT)
endif()
