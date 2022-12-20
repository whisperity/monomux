#ifndef MONOMUX_CONFIG_H
#define MONOMUX_CONFIG_H

/* This file contains the CMake-level configuration variables that are exposed
 * to the compilers executed.
 */

/* If set, the built binaries will be composed of several shared library (.so,
 * .dll) for reusability in other projects.
 *
 * Turn off to improve optimisations if Monomux is only used as the reference
 * implementation tool (normally it is).
 */
#cmakedefine MONOMUX_BUILD_SHARED_LIBS

/* If set, the built binary will be created from a single translation unit of
 * the entire project.
 *
 * This allows for greater optimisations to be performed by the compiler at the
 * expence of a substantial drag on compiler performance.
 */
#cmakedefine MONOMUX_BUILD_UNITY

/* If set, the built binary will contain some additional log outputs that are
 * needed for verbose debugging of the project.
 *
 * Turn off to cut down further on the binary size for production.
 */
#cmakedefine MONOMUX_NON_ESSENTIAL_LOGS

/* The system platform (string) that the current build is being done on. */
#define MONOMUX_PLATFORM "${MONOMUX_PLATFORM}"

/* Define some constants for the platforms that are supported.
 * These values will be used (exactly one of them) in MONOMUX_PLATFORM_ID
 * to support a numerical comparison (e.g.,
 *     #if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_Unix
 * ) for more complex code where a simple #if(n)def does not suffice.
 */
#define MONOMUX_PLATFORM_ID_Unsupported 0
#define MONOMUX_PLATFORM_ID_Unix 1

/* The current platform's ID. Always selected from the MONOMUX_PLATFORM_ID_*
 * macros at build time.
 *
 * This supports a numerical comparison for cases where a simple #if(n)def
 * is not sufficient:
 *     #if MONOMUX_PLATFORM_ID == MONOMUX_PLATFORM_Unix
 */
/* clang-format off */
#define MONOMUX_PLATFORM_ID MONOMUX_PLATFORM_ID_${MONOMUX_PLATFORM}
/* clang-format on */

/* If set, the PLATFORM is "Unix". Shorthand for the == check on the ID. */
#cmakedefine MONOMUX_PLATFORM_UNIX

/* The build type for the current build. */
#define MONOMUX_BUILD_TYPE "${CMAKE_BUILD_TYPE}"

#endif /* MONOMUX_CONFIG_H */
