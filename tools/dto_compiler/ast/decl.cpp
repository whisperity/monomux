/* SPDX-License-Identifier: LGPL-3.0-only */
#include <sstream>

#include "decl.hpp"

namespace monomux::tools::dto_compiler::ast
{

namespace
{

void print_ident(std::ostringstream& OS, std::size_t Indent)
{
  if (Indent == 0)
    OS << '.';

  while (Indent > 1)
  {
    OS << "|  ";
    --Indent;
  }
  if (Indent == 1)
  {
    OS << "|- ";
  }
}

} // namespace

[[nodiscard]] const decl*
decl_context::lookup(std::string_view Identifier) const noexcept
{
  auto LookupInChain = [](const decl_context* C,
                          std::string_view I) -> const named_decl* {
    for (C = &C->first_in_chain(); C != nullptr; C = C->next())
    {
      if (const auto* FoundChild =
            dynamic_cast<const named_decl*>(C->lookup_in_current(I)))
        return FoundChild;
    }

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

[[nodiscard]] const decl*
decl_context::lookup_in_current(std::string_view Identifier) const noexcept
{
  for (const auto& NodeUPtr : Children)
  {
    if (const auto* ND = dynamic_cast<const named_decl*>(NodeUPtr.get());
        ND && ND->get_identifier() == Identifier)
      return ND;
  }
  return nullptr;
}

void decl_context::dump_children(std::ostringstream& OS,
                                 std::size_t Depth) const
{
  // This should only print the *local* children as the dump is expected to be
  // in-order transitive.
  for (const auto& Child : Children)
  {
    print_ident(OS, Depth);
    Child->dump(OS, Depth + 1);
  }
}

#define MONOMUX_DECL_DUMP(TYPE)                                                \
  void TYPE::dump(std::ostringstream& OS, std::size_t Depth) const

MONOMUX_DECL_DUMP(decl) {}

MONOMUX_DECL_DUMP(comment_decl)
{
  static constexpr std::size_t CommentPrintLength = 64;
  OS << "CommentDecl " << this << ' ';
  OS << (Comment.is_block_comment() ? "block " : "line  ");
  OS << Comment.get_comment().substr(0, CommentPrintLength);
  OS << '\n';
}

MONOMUX_DECL_DUMP(named_decl) {}

MONOMUX_DECL_DUMP(namespace_decl)
{
  OS << "NamespaceDecl " << this << ' ' << get_identifier() << ' ';
  if (prev())
    OS << "prev " << dynamic_cast<const namespace_decl*>(prev()) << ' ';
  if (next())
    OS << "next " << dynamic_cast<const namespace_decl*>(next()) << ' ';
  OS << '\n';
  dump_children(OS, Depth);
}

MONOMUX_DECL_DUMP(value_decl) {}

MONOMUX_DECL_DUMP(literal_decl) {}

#undef MONOMUX_DECL_DUMP

} // namespace monomux::tools::dto_compiler::ast
