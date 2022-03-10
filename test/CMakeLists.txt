include(FetchContent)
FetchContent_Declare(
  googletest
  URL http://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)

set(BUILD_GMOCK OFF)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Do not add GoogleTest stuff as our install() target...
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)  # option(INSTALL_GTEST) would override.
set(INSTALL_GTEST OFF)

# Actually build GoogleTest.
FetchContent_MakeAvailable(googletest)

include(CTest)

add_executable(monomux_tests
  main.cpp

  control/MessageSerialisationTest.cpp
  )
target_include_directories(monomux_tests PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  )
target_link_libraries(monomux_tests PRIVATE
  monomuxCore
  )
target_link_libraries(monomux_tests PUBLIC
  gtest_main
  )

add_test(NAME tests
  COMMAND monomux_tests
  )

add_custom_target(check
  COMMAND env CTEST_OUTPUT_ON_FAILURE=1 GTEST_COLOR=1 ${CMAKE_CTEST_COMMAND}
  DEPENDS monomux_tests)
