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
