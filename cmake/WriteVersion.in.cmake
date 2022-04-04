# This trampoline script generates a C header containing version information.
# This script is called automatically, see Version.cmake in the source tree.
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake"
  )

set(VERSIONING_FROM_TRAMPOLINE ON)
set(VERSION_HEADER_TEMPLATE    "${VERSION_HEADER_TEMPLATE}")
set(VERSION_HEADER_RESULT      "${VERSION_HEADER_RESULT}")
set(VERSION_TXT_RESULT         "${VERSION_TXT_RESULT}")

include(MonomuxVersion)

write_version_header()
