#ifndef CMAKE_VERSION_MODULE_H
#define CMAKE_VERSION_MODULE_H

/* The main version number components. */
#define VERSION_MAJOR "${VERSION_MAJOR}"
#define VERSION_MINOR "${VERSION_MINOR}"
#define VERSION_PATCH "${VERSION_PATCH}"

/* The "tweak" version number is a sub-release indicator.
 * This is usually a direct build number.
 */
#define VERSION_TWEAK "${VERSION_TWEAK}"


/* Whether the versioning system uncovered additional detail. */
#cmakedefine VERSION_HAS_EXTRAS

#ifdef VERSION_HAS_EXTRAS

/* The amount of commits since the tagged version. */
#define VERSION_OFFSET "${VERSION_OFFSET}"

/* The hash of the current commit. */
#define VERSION_COMMIT "${VERSION_COMMIT}"

/* Whether there were local, uncommitted changes during build. */
#cmakedefine01 VERSION_DIRTY

#endif /* VERSION_HAS_EXTRAS */

#endif /* CMAKE_VERSION_MODULE_H */
