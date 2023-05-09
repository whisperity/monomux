# SPDX-License-Identifier: GPL-3.0-only
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(MONOMUX_BUILD_TESTS_DEFAULT ON)
else()
  set(MONOMUX_BUILD_TESTS_DEFAULT OFF)
endif()

set(MONOMUX_BUILD_TESTS ${MONOMUX_BUILD_TESTS_DEFAULT} CACHE BOOL
  "Whether to build the test framework when building the project.")

if (MONOMUX_BUILD_TESTS)
  if (MONOMUX_BUILD_UNITY)
    message(WARNING "Unity build is not compatible with testing, but MONOMUX_BUILD_TESTS was supplied. Prioritising unity build and disabling tests...")
    set(MONOMUX_BUILD_TESTS OFF)
    return()
  endif()

  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL http://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
  )

  set(BUILD_GMOCK OFF)

  # For Windows: Prevent overriding the parent project's compiler/linker
  # settings.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  # Do not add GoogleTest stuff as our install() target...
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)  # option(INSTALL_GTEST) would override.
  set(INSTALL_GTEST OFF)

  # Actually build GoogleTest.
  FetchContent_MakeAvailable(googletest)

  include(CTest)

  add_executable(monomux_tests
    main.cpp

    adt/MetaprogrammingTest.cpp
    adt/RingBufferTest.cpp
    adt/SmallIndexMapTest.cpp
    adt/StateMachineTest.cpp
    message/MessageSerialisationTest.cpp
    )
  target_include_directories(monomux_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    )
  target_link_libraries(monomux_tests PRIVATE
    monomuxCore
    monomuxImplementation
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
else()
  add_custom_target(check
    COMMAND echo "Testing is not supported in this build. Set MONOMUX_BUILD_TESTS=ON or create a Debug build."
    COMMAND exit 1
    )
endif()
