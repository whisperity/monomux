/* SPDX-License-Identifier: LGPL-3.0-only */
#include <algorithm>
#include <sstream>

#include "decl.hpp"

namespace monomux::tools::dto_compiler::ast
{

const named_decl*
decl_context::lookup_in_current(std::string_view Identifier) const noexcept
{
  auto It = std::find_if(
    Children.begin(), Children.end(), [Identifier](const auto& NodeUniquePtr) {
      if (const auto* ND = dynamic_cast<const named_decl*>(NodeUniquePtr.get()))
        return ND->get_identifier() == Identifier;
      return false;
    });
  return It != Children.end() ? dynamic_cast<const named_decl*>(It->get())
                              : nullptr;
}

const named_decl*
decl_context::lookup(std::string_view Identifier) const noexcept
{
  auto LookupInChain = [](const decl_context* C,
                          std::string_view I) -> const named_decl* {
    for (C = &C->first_in_chain(); C; C = C->next())
      if (const auto* FoundChild = C->lookup_in_current(I))
        return FoundChild;
    return nullptr;
  };

  std::string_view IdentifierUpToScopeOperator =
    Identifier.substr(0, Identifier.find("::"));

  if (IdentifierUpToScopeOperator == Identifier)
  {
    // No scope operator present in the lookup, perform a direct search.
    return LookupInChain(this, IdentifierUpToScopeOperator);
  }
  // Otherwise, we found some sort of scope directive in the name.
  if (const auto* Scope = dynamic_cast<const namespace_decl*>(
        LookupInChain(this, IdentifierUpToScopeOperator)))
  {
    const auto* ScopeContext = dynamic_cast<const decl_context*>(Scope);
    std::string_view IdentifierFollowingScopeOperator =
      Identifier.substr(Identifier.find("::") + 2);
    return ScopeContext->lookup(IdentifierFollowingScopeOperator);
  }

  return nullptr;
}

const named_decl*
decl_context::lookup_with_parents(std::string_view Identifier) const noexcept
{
  const auto* D = lookup(Identifier);
  if (D)
    return D;
  if (parent())
    return parent()->lookup_with_parents(Identifier);
  return nullptr;
}

const namespace_decl* namespace_decl::get_outermost_namespace() const noexcept
{
  if (!parent())
    return this;
  if (const auto* ParentNSD = dynamic_cast<const namespace_decl*>(parent()))
    return ParentNSD->get_outermost_namespace();
  return this;
}

} // namespace monomux::tools::dto_compiler::ast
