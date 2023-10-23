#include <iostream>
#include <sstream>

#include "decl.hpp"
#include "expr.hpp"
#include "type.hpp"

#include "dump.hpp"

namespace monomux::tools::dto_compiler::ast
{

void decl::dump(std::size_t Depth) const
{
  std::ostringstream OS;
  dump(OS, Depth);
  std::cerr << OS.str() << std::endl;
}

void type::dump(std::size_t Depth) const
{
  std::ostringstream OS;
  dump(OS, Depth);
  std::cerr << OS.str() << std::endl;
}

void decl_context::dump_children(std::size_t Depth) const
{
  std::ostringstream OS;
  dump_children(OS, Depth);
  std::cerr << OS.str() << std::endl;
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

#define MONOMUX_AST_DUMP(TYPE)                                                 \
  void TYPE::dump(std::ostringstream& OS, std::size_t Depth) const

#define MONOMUX_DUMP_CHILD(ACCESSOR)                                           \
  {                                                                            \
    print_ident(OS, Depth);                                                    \
    ACCESSOR->dump(OS, Depth + 1);                                             \
  }

MONOMUX_AST_DUMP(decl) {}

MONOMUX_AST_DUMP(comment_decl)
{
  static constexpr std::size_t CommentPrintLength = 64;
  OS << "CommentDecl " << this << ' ';
  OS << (Comment.is_block_comment() ? "block " : "line  ");
  OS << Comment.get_comment().substr(0, CommentPrintLength);
  if (Comment.get_comment().size() > CommentPrintLength)
    OS << "...";
  OS << '\n';
}

MONOMUX_AST_DUMP(named_decl) {}

MONOMUX_AST_DUMP(type_decl)
{
  OS << "TypeDecl " << this << ' ' << get_identifier() << '\n';
  MONOMUX_DUMP_CHILD(get_type());
}

MONOMUX_AST_DUMP(namespace_decl)
{
  OS << "NamespaceDecl " << this << ' ' << get_identifier() << ' ';
  if (prev())
    OS << "prev " << dynamic_cast<const namespace_decl*>(prev()) << ' ';
  if (next())
    OS << "next " << dynamic_cast<const namespace_decl*>(next()) << ' ';
  OS << '\n';
  dump_children(OS, Depth);
}

MONOMUX_AST_DUMP(value_decl) {}

MONOMUX_AST_DUMP(literal_decl)
{
  OS << "LiteralDecl " << this << ' ' << get_identifier() << ' ';
  OS << get_type()->get_identifier() << '\n';
  MONOMUX_DUMP_CHILD(get_value());
}

MONOMUX_AST_DUMP(type) {}

MONOMUX_AST_DUMP(integral_type)
{
  OS << "BuiltinType " << this << ' ' << get_generated_identifier() << '\n';
}

MONOMUX_AST_DUMP(expr) {}

MONOMUX_AST_DUMP(unsigned_integral_literal)
{
  OS << "IntegralLiteral " << this << " unsigned " << get_value() << '\n';
}

#undef MONOMUX_AST_DUMP

} // namespace monomux::tools::dto_compiler::ast
