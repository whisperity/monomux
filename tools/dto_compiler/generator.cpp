/* SPDX-License-Identifier: LGPL-3.0-only */
// #include <algorithm>
// #include <cassert>
// #include <cstring>
// #include <optional>
// #include <sstream>
// #include <type_traits>
#include <utility>

#ifndef NDEBUG
#include <iostream>
#endif /* !NDEBUG */

// #include "monomux/Debug.h"
// #include "monomux/adt/SmallIndexMap.hpp"
// #include "monomux/adt/scope_guard.hpp"
// #include "monomux/unreachable.hpp"

#include "ast/decl.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"
#include "dto_unit.hpp"

#include "generator.hpp"

namespace monomux::tools::dto_compiler
{

generator::generator(std::unique_ptr<dto_unit>&& DTO,
                     // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                     std::ostream& InterfaceStream,
                     std::ostream& ImplementationStream)
  : DTO(std::move(DTO)), InterfaceOut(InterfaceStream),
    ImplementationOut(ImplementationStream)
{}

namespace
{

void generate_usage_for_type(std::ostream& Stream, const ast::type& Node);
void generate_expression(std::ostream& Stream, const ast::expr& Node);

#define MONOMUX_GENERATE_LEAF_TYPE(TYPE)                                       \
  if (const auto* CastNode = dynamic_cast<const ast::TYPE*>(&Node))            \
  {                                                                            \
    interface_for(IfStream, *CastNode);                                        \
    implementation_for(ImplStream, *CastNode);                                 \
    continue;                                                                  \
  }

#define MONOMUX_GENERATE_USAGE_LEAF_TYPE(TYPE)                                 \
  if (const auto* CastNode = dynamic_cast<const ast::TYPE*>(&Node))            \
  {                                                                            \
    usage_for(Stream, *CastNode);                                              \
    return;                                                                    \
  }

#define MONOMUX_GEN_IF(TYPE)                                                   \
  void interface_for(std::ostream& O, const ast::TYPE& Node)
#define MONOMUX_GEN_IMPL(TYPE)                                                 \
  void implementation_for(std::ostream& O, const ast::TYPE& Node)
#define MONOMUX_GEN_USE(TYPE)                                                  \
  void usage_for(std::ostream& O, const ast::TYPE& Node)

MONOMUX_GEN_USE(integral_type) { O << Node.get_generated_identifier(); }

MONOMUX_GEN_USE(unsigned_integral_literal) { O << Node.get_value(); }

MONOMUX_GEN_IF(comment_decl)
{
  const auto& I = Node.get_comment_info();
  O << (I.is_block_comment() ? "/*" : "//");
  O << I.get_comment();
  O << (I.is_block_comment() ? "*/" : "") << '\n';
}
MONOMUX_GEN_IMPL(comment_decl) { interface_for(O, Node); }

MONOMUX_GEN_IF(literal_decl)
{
  O << "static constexpr const ";
  generate_usage_for_type(O, *Node.get_type());
  O << ' ' << Node.get_identifier() << " = ";
  generate_expression(O, *Node.get_value());
  O << ';' << '\n';
}
MONOMUX_GEN_IMPL(literal_decl) {}

#undef MONOMUX_GEN_IF
#undef MONOMUX_GEN_IMPL
#undef MONOMUX_GEN_USE

void generate_usage_for_type(std::ostream& Stream, const ast::type& Node)
{
  MONOMUX_GENERATE_USAGE_LEAF_TYPE(integral_type)
}

void generate_expression(std::ostream& Stream, const ast::expr& Node)
{
  MONOMUX_GENERATE_USAGE_LEAF_TYPE(unsigned_integral_literal)
}

void generate_for_context(
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  std::ostream& IfStream,
  std::ostream& ImplStream,
  const ast::decl_context& Context)
{
  for (auto NodeIt = Context.children_begin(); NodeIt != Context.children_end();
       ++NodeIt)
  {
    const ast::decl& Node = *NodeIt;

    if (const auto* NSD = dynamic_cast<const ast::namespace_decl*>(&Node))
    {
      std::ostringstream Head;
      Head << "\nnamespace " << NSD->get_identifier() << "\n{\n\n";

      IfStream << Head.str();
      ImplStream << Head.str();

      generate_for_context(IfStream, ImplStream, *NSD);

      const std::string Foot = "\n}\n";
      IfStream << Foot;
      ImplStream << Foot;

      continue;
    }

    MONOMUX_GENERATE_LEAF_TYPE(comment_decl)
    MONOMUX_GENERATE_LEAF_TYPE(literal_decl)
  }
}

#undef MONOMUX_GENERATE_LEAF_TYPE

} // namespace

void generator::generate()
{
  InterfaceOut << DTO->get_interface_preamble() << '\n';
  ImplementationOut << DTO->get_implementation_preamble() << '\n';

  generate_for_context(InterfaceOut, ImplementationOut, DTO->get_root());
}

} // namespace monomux::tools::dto_compiler
