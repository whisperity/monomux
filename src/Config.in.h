#ifndef MONOMUX_CONFIG_H
#define MONOMUX_CONFIG_H

/* If set, the built binaries will be composed of several shared library (.so,
 * .dll) for reusability in other projects.
 *
 * Turn off to improve optimisations if Monomux is only used as the reference
 * implementation tool (normally it is).
 */
#cmakedefine MONOMUX_BUILD_SHARED_LIBS

/* If set, the built binary will contain some additional log outputs that are
 * needed for verbose debugging of the project.
 *
 * Turn off to cut down further on the binary size for production.
 */
#cmakedefine MONOMUX_NON_ESSENTIAL_LOGS

/* The build type for the current build. */
#define MONOMUX_BUILD_TYPE "${CMAKE_BUILD_TYPE}"

#endif /* MONOMUX_CONFIG_H */
