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

/* The system platform that the current build is being done on. */
#define MONOMUX_PLATFORM "${MONOMUX_PLATFORM}"

/* If set, the PLATFORM is "Unix". */
#cmakedefine MONOMUX_PLATFORM_UNIX

/* The build type for the current build. */
#define MONOMUX_BUILD_TYPE "${CMAKE_BUILD_TYPE}"

#endif /* MONOMUX_CONFIG_H */
