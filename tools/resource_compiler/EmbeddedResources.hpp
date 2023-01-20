/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once

#ifdef __cplusplus
#include <cstdint>
#else /* !__cplusplus */
#include <stdint.h>
#endif /* __cplusplus */

#if __cplusplus >= 201703L
#include <optional>
#include <string_view>
#elif __cplusplus >= 201103L
#include <string>
#endif /* __cplusplus 11 */

#include "./EmbeddedResources_Config.h"

#define CONCAT_1(A, B) A##_##B
#define CONCAT(A, B) CONCAT_1(A, B)
#define EMBEDDED_RESOURCES_ARRAY_REF                                           \
  CONCAT(EMBEDDED_RESOURCES_PROJECT_NAMESPACE, array_ref)

#ifdef __cplusplus
#define STDLIB_TYPE(T) std::T
#else /* ! __cplusplus */
#define STDLIB_TYPE(T) T
#endif /* __cplusplus */

/* NOLINTBEGIN(readability-identifier-naming) */
/// Weak reference to a contiguous buffer with a known size.
struct EMBEDDED_RESOURCES_ARRAY_REF
{
  const STDLIB_TYPE(uint8_t) * begin;
  const STDLIB_TYPE(uint8_t) * end;
  STDLIB_TYPE(size_t) size;
};
/* NOLINTEND(readability-identifier-naming) */

#undef STDLIB_TYPE

#define EMBEDDED_RESOURCES_GETTER                                              \
  CONCAT(EMBEDDED_RESOURCES_PROJECT_NAMESPACE, get_resource)

/// Retrieve the raw buffer for the resource embedded under the name
/// \p Identifier. If such resource is not found, an empty \p ArrayRef is
/// returned instead.
#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */
  struct EMBEDDED_RESOURCES_ARRAY_REF
  EMBEDDED_RESOURCES_GETTER(const char* Identifier);

#ifdef __cplusplus
// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace EMBEDDED_RESOURCES_PROJECT_NAMESPACE
{

/// Contains the data structures and entry points to access resources embedded
/// automatically by the build system's \p resource_compiler tool.
namespace resources
{

/// Weak reference to a contiguous buffer with a known size.
using ArrayRef = EMBEDDED_RESOURCES_ARRAY_REF;

#if __cplusplus >= 201703L

/// Retrieve the raw buffer for the resource embedded under the name
/// \p Identifier. If such resource is not found, \p std::nullopt is returned
/// instead.
std::optional<std::string_view> get(std::string_view Identifier) noexcept;

#elif __cplusplus >= 201103L /* __cplusplus 17 */

/// Retrieve the raw buffer for the resource embedded under the name
/// \p Identifier. If such resource is not found, an empty \p ArrayRef is
/// returned instead.
ArrayRef get(const std::string& Identifier) noexcept;

#endif /* __cplusplus 11 */

} // namespace resources

// NOLINTNEXTLINE(llvm-namespace-comment)
} // namespace EMBEDDED_RESOURCES_PROJECT_NAMESPACE

#endif /* __cplusplus */

#undef EMBEDDED_RESOURCES_ARRAY_REF
#undef EMBEDDED_RESOURCES_GETTER
#undef CONCAT
#undef CONCAT_1
