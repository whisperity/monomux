/* SPDX-License-Identifier: LGPL-3.0-only */

#ifndef MONOMUX_VERSION_H
#define MONOMUX_VERSION_H

/* The main version number components. */
#define MONOMUX_VERSION_MAJOR "${VERSION_MAJOR}"
#define MONOMUX_VERSION_MINOR "${VERSION_MINOR}"
#define MONOMUX_VERSION_PATCH "${VERSION_PATCH}"

/* The "tweak" version number is a sub-release indicator.
 * This is usually a direct build number.
 */
#define MONOMUX_VERSION_TWEAK "${VERSION_TWEAK}"

/* Whether the versioning system uncovered additional detail. */
#define MONOMUX_VERSION_HAS_EXTRAS "${VERSION_HAS_EXTRAS}"

#ifdef MONOMUX_VERSION_HAS_EXTRAS

/* The amount of commits since the tagged version. */
#define MONOMUX_VERSION_OFFSET "${VERSION_OFFSET}"

/* The hash of the current commit. */
#define MONOMUX_VERSION_COMMIT "${VERSION_COMMIT}"

/* Whether there were local, uncommitted changes during build. */
#define MONOMUX_VERSION_DIRTY "${VERSION_DIRTY}"

#endif /* MONOMUX_VERSION_HAS_EXTRAS */

#endif /* MONOMUX_VERSION_H */
