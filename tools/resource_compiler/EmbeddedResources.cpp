/* SPDX-License-Identifier: LGPL-3.0-only */
/* N.b.: the licence conditions as described by the licence reference above are
 * only applicable to the developer-written code available in this file, in its
 * original, "source file" form.
 *
 * This file is an input to an automated process that uses the framework
 * present in this file to generate embedded content that is available
 * programmatically. "Embedded content" is obtained from the file system of the
 * developer machine during build, and each piece of embedded content might be
 * licenced under a different licence than the framework of this file!
 *
 * If the following line (the first actual code line) of the file is the
 * EMBEDDED_RESOURCES_REPLACE_THIS_WITH dummmy maro definition, then you are
 * viewing the source code version of this file. Otherwise, embedded content is
 * present in an encoded form.
 */
#define EMBEDDED_RESOURCES_REPLACE_THIS_WITH(WITH)

// FIXME: This "library" does not support compiling multiple resource images
// and having them loaded as multiple shared objects.

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

EMBEDDED_RESOURCES_REPLACE_THIS_WITH("RealHeaderInclude");
#ifndef EMBEDDED_RESOURCES_PROJECT_NAMESPACE
#include "EmbeddedResources.hpp"
#endif /* EMBEDDED_RESOURCES_PROJECT_NAMESPACE */

namespace
{

/// Weak pointer to a buffer with only a starting point and a size known.
struct ArrayPtr
{
  const std::uint8_t* Buffer;
  std::size_t Size;

#if __cplusplus >= 201703L
  [[nodiscard]]
#endif /* __cplusplus 17 */
  const std::uint8_t*
  end() const noexcept
  {
    return Buffer + Size;
  }
};

/// Container meta-structure for the resource system.
struct Resources
{
  std::unordered_map<std::string, ArrayPtr> Map;
};

/// Instance manager, spawned by \p init_resources().
std::unique_ptr<Resources> StaticPtr;

/// Initialise the resource system by loading all the resource buffer pointers
/// into an accessible data structure.
///
/// \note This should not be called by users directly!
Resources& init_resources(); // NOLINT(readability-identifier-naming)

} // namespace

// NOLINTNEXTLINE(modernize-concat-nested-namespaces)
namespace EMBEDDED_RESOURCES_PROJECT_NAMESPACE
{

namespace resources
{

ArrayRef get(const std::string& Identifier) noexcept
{
  if (!StaticPtr)
    init_resources();

  if (auto It = StaticPtr->Map.find(Identifier); It != StaticPtr->Map.end())
    return {
      It->second.Buffer, It->second.Buffer + It->second.Size, It->second.Size};

  return {nullptr, nullptr, 0};
}

std::optional<std::string_view> get(std::string_view Identifier) noexcept
{
  ArrayRef AR = get(std::string{Identifier});
  if (AR.begin == nullptr)
    return std::nullopt;

  static_assert(sizeof(std::uint8_t) == sizeof(char),
                "Buffer backing expects 8-bit bytes");
  return std::string_view{reinterpret_cast<const char*>(AR.begin), AR.size};
}

} // namespace resources

// NOLINTNEXTLINE(llvm-namespace-comment)
} // namespace EMBEDDED_RESOURCES_PROJECT_NAMESPACE

#define CONCAT_1(A, B) A##_##B
#define CONCAT(A, B) CONCAT_1(A, B)
#define EMBEDDED_RESOURCES_ARRAY_REF                                           \
  CONCAT(EMBEDDED_RESOURCES_PROJECT_NAMESPACE, array_ref)
#define EMBEDDED_RESOURCES_GETTER                                              \
  CONCAT(EMBEDDED_RESOURCES_PROJECT_NAMESPACE, get_resource)

#ifdef __cplusplus
extern "C"
#endif /* __cplusplus */
  struct EMBEDDED_RESOURCES_ARRAY_REF
  EMBEDDED_RESOURCES_GETTER(const char* Identifier)
{
  using namespace EMBEDDED_RESOURCES_PROJECT_NAMESPACE::resources;
#if __cplusplus >= 201703L
  std::optional<std::string_view> Res = get(std::string_view{Identifier});
  if (!Res)
    return EMBEDDED_RESOURCES_ARRAY_REF{nullptr, nullptr, 0};
  return EMBEDDED_RESOURCES_ARRAY_REF{
    reinterpret_cast<const std::uint8_t*>(Res->begin()),
    reinterpret_cast<const std::uint8_t*>(Res->end()),
    Res->size()};
#elif __cplusplus >= 201103L /* __cplusplus 17 */
  return get(std::string{Identifier});
#endif                       /* __cplusplus 11 */
}

/* NOLINTBEGIN(bugprone-macro-parantheses) */
// Note that the resulting type is deliberately **not** constexpr here.
// Access to the resource is facilitated through a run-time identifier-based
// getter anyway, and using "constexpr std::array<...>" would reult in some
// weird duplication of the buffer in the built object, blowing up the memory
// footprint.
#define RESOURCE_BUFFER(SYMBOL_NAME, SIZE)                                     \
  std::array<std::uint8_t, SIZE> SYMBOL_NAME
#define RESOURCE_INIT(DEVELOPER_NAME, SYMBOL_NAME)                             \
  StaticPtr->Map[DEVELOPER_NAME] =                                             \
    ArrayPtr{(SYMBOL_NAME).data(), (SYMBOL_NAME).size()};
/* NOLINTEND(bugprone-macro-parantheses) */

/* Following this line, except for the #undef directives at the very end of the
 * file, follows content that is **generated** from data that was available
 * during the build process. Note that these contents, if present in the output
 * file as opposed to viewing this file as a source, might be licenced under
 * different terms than that of the hand-written initial "source" version of
 * this file.
 */

namespace
{

  EMBEDDED_RESOURCES_REPLACE_THIS_WITH("DataDirectives");

  // NOLINTNEXTLINE(readability-identifier-naming)
  Resources& init_resources()
  {
    if (!StaticPtr)
      StaticPtr = std::make_unique<Resources>();

    EMBEDDED_RESOURCES_REPLACE_THIS_WITH("EntryEmplaces");

    return *StaticPtr;
  }

} // namespace

#undef EMBEDDED_RESOURCES_ARRAY_REF
#undef EMBEDDED_RESOURCES_GETTER
#undef CONCAT
#undef CONCAT_1

#undef RESOURCE_BUFFER
#undef RESOURCE_INIT
#undef EMBEDDED_RESOURCES_REPLACE_THIS_WITH
