/* SPDX-License-Identifier: LGPL-3.0-only */
#include <cassert>
#include <numeric>

#ifndef NDEBUG
#include <iostream>
#endif /* !NDEBUG */

#include "monomux/Debug.h"
#include "monomux/adt/scope_guard.hpp"

#include "ast/decl.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"
#include "dto_unit.hpp"
#include "lexer.hpp"

#include "parser.hpp"

namespace monomux::tools::dto_compiler
{

namespace
{

// NOLINTNEXTLINE(fuchsia-statically-constructed-objects)
const parser::error_info EmptyError{};

std::string join_scoped_identifiers(const std::vector<std::string>& Identifiers)
{
  if (Identifiers.size() == 1)
    return Identifiers.front();
  return std::accumulate(Identifiers.begin(),
                         Identifiers.end(),
                         std::string{},
                         [](std::string Sum, std::string Elem) {
                           return std::move(Sum) + "::" + std::move(Elem);
                         });
}

/// Returns the top-level \p namespace_decl (which \p parent is the root)
/// that corresponds to \p DC.
const ast::namespace_decl*
get_top_namespace_in_root_for(const ast::decl_context* DC)
{
  const auto* NSD = dynamic_cast<const ast::namespace_decl*>(DC);
  if (!NSD)
    return nullptr;
  return NSD->get_outermost_namespace();
}

} // namespace

#define INFO(TOKEN_KIND)                                                       \
  const auto& Info = Lexer.get_token_info<token::TOKEN_KIND>();

std::vector<std::string> parser::parse_potentially_scoped_identifier()
{
  std::vector<std::string> Identifiers;
  Identifiers.emplace_back("");
  bool PreviousTokenWasIdentifier = false;
  while (true)
  {
    token T = get_current_token();
    if (T == token::Scope)
    {
      PreviousTokenWasIdentifier = false;

      Identifiers.emplace_back("");
    }
    else if (T == token::Identifier)
    {
      if (PreviousTokenWasIdentifier)
        // Two Identifier tokens following each other directly without a
        // Scope token inbetween do not compose the same identifier.
        return Identifiers;
      PreviousTokenWasIdentifier = true;

      INFO(Identifier);
      Identifiers.back() = Info->Identifier;
    }
    else
    {
      if (Identifiers.back().empty())
        set_error_to_current_token("Invalid identifier sequence ended in "
                                   "non-identifier.");
      return Identifiers;
    }

    (void)get_next_token();
  }
}

bool parser::parse_namespace()
{
  assert(get_current_token() == token::Namespace && "Expected 'namespace'");
  (void)get_next_token(); // Consume 'namespace', begin identifiers...
  std::vector<std::string> Identifiers = parse_potentially_scoped_identifier();

  if (get_current_token() != token::LBrace)
  {
    set_error_to_current_token("Expected '{' after namespace identifier "
                               "declaration");
    return false;
  }

  restore_guard G{DeclContext};
  auto* NSD = [this,
               &Identifiers,
               Context = this->DeclContext]() mutable -> ast::namespace_decl* {
    for (std::string NextScope : Identifiers)
    {
      ast::decl* ExistingD = Context->lookup(NextScope);
      auto* ExistingNSD = dynamic_cast<ast::namespace_decl*>(ExistingD);
      if (ExistingD && !ExistingNSD)
      {
        set_error_to_current_token(
          std::string{"Attempted to create a namespace '"} + NextScope +
          std::string{"' while a non-namespace with the same name already "
                      "exists in the current scope"});
        return nullptr;
      }
      auto* NSD =
        Context->emplace_child<ast::namespace_decl>(std::move(NextScope));
      if (ExistingNSD)
        ExistingNSD->last_in_chain().add_to_chain(*NSD);
      Context = NSD;
    }

    return dynamic_cast<ast::namespace_decl*>(Context);
  }();
  if (!NSD)
    return false;

  DeclContext = NSD;
  (void)get_next_token();
  bool Inner = parse();

  if (Inner && get_current_token() != token::RBrace)
  {
    set_error_to_current_token("Parsing of a 'namespace' ended without a '}'");
    return false;
  }

  return Inner;
}

void parser::parse_constant()
{
  assert(get_current_token() == token::Literal && "Expected 'literal'");

  (void)get_next_token(); // Consume 'literal', begin identifiers...
  error_info NoTypeError = prepare_error();
  std::vector<std::string> TypeIdentifier =
    parse_potentially_scoped_identifier();
  if (TypeIdentifier.empty())
  {
    set_error_to_current_token(
      "A constant declaration must identify the type the constant has");
    return;
  }

  const auto* TD = [&]() -> const ast::type_decl* {
    std::string TypeId = join_scoped_identifiers(TypeIdentifier);
    const auto* TD =
      dynamic_cast<ast::type_decl*>(DeclContext->lookup_with_parents(TypeId));
    if (!TD)
    {
      if (auto* T = ast::type::try_as_conjured(*ParseUnit, TypeId))
        TD =
          get_unit().get_root().emplace_child_in_chain_before<ast::type_decl>(
            get_top_namespace_in_root_for(DeclContext), TypeId, T);

      if (!TD)
      {
        set_error(std::move(NoTypeError),
                  std::string{"Undefined type '"} + std::move(TypeId) +
                    std::string{"'"});
        return nullptr;
      }
    }

    return TD;
  }();
  if (!TD)
    return;

  if (get_current_token() == token::Eq)
  {
    set_error_to_current_token("Expected precisely 2 identifiers for the type "
                               "and the name of the constant");
    return;
  }
  if (get_current_token() != token::Identifier)
  {
    set_error_to_current_token(std::string{"Unexpected '"} +
                               std::string{to_string(get_current_token())} +
                               std::string{"' instead of the constant's name"});
    return;
  }

  std::string Identifier = [this] {
    INFO(Identifier);
    return Info->Identifier;
  }();

  if (const auto* ND = DeclContext->lookup(Identifier))
  {
    set_error_to_current_token(std::string{"Multiple definitions for '"} +
                               Identifier +
                               std::string{"' in the current scope"});
    return;
  }

  if (get_next_token() != token::Eq)
  {
    set_error_to_current_token("Expected '='");
    return;
  }

  ast::expr* InitExpr = parse_expression();
  if (!InitExpr)
    return;

  error_info NoSemi = prepare_error();
  if (get_next_token() != token::Semicolon)
  {
    set_error(std::move(NoSemi),
              "All non-scope declarations must be terminated by ';'");
    return;
  }

  DeclContext->emplace_child<ast::literal_decl>(
    std::move(Identifier), TD->get_type(), InitExpr);
}

ast::expr* parser::parse_expression()
{
  token Init = get_next_token();
  switch (Init)
  {
    case token::Integral:
    {
      INFO(Integral);
      return new ast::unsigned_integral_literal{Info->Value};
    }

    default:
      set_error_to_current_token(std::string{"Unexpected '"} +
                                 std::string{to_string(get_current_token())} +
                                 std::string{"' instead of an expression"});
      return nullptr;
  }
}

bool parser::parse()
{
  ast::decl_context* CurrentContext = DeclContext;

  while (true)
  {
    switch (get_current_token())
    {
      case token::BeginningOfFile:
      {
        (void)get_next_token();
        continue;
      }

      case token::EndOfFile:
        return true;

      case token::SyntaxError:
        return false;

      default:
        set_error_to_current_token(std::string{"Unexpected '"} +
                                   std::string{to_string(get_current_token())} +
                                   std::string{"' encountered while parsing."});
        return false;

      case token::RBrace:
        if (CurrentContext == &ParseUnit->get_root())
        {
          set_error_to_current_token("'}' does not close anything here");
          return false;
        }
        return true;

      case token::Comment:
      {
        INFO(Comment);
        CurrentContext->emplace_child<ast::comment_decl>(
          ast::detail::comment{Info->IsBlockComment, Info->Comment});
        break;
      }

      case token::Namespace:
        parse_namespace();
        break;

      case token::Literal:
        parse_constant();
        break;

      case token::Function:
      case token::Record:
        set_error_to_current_token("TBD Keyword token.");
        return false;
        break;
    }

    if (!has_error())
      (void)get_next_token();
    else
      return false;
  }

  return true;
}

parser::parser(lexer& Lexer)
  : Lexer(Lexer), ParseUnit(std::make_unique<dto_unit>()),
    DeclContext(&ParseUnit->get_root())
{}

parser::~parser() { DeclContext = nullptr; }

const parser::error_info& parser::get_error() const noexcept
{
  assert(has_error() && "Invalid call to 'get_error' if no error exists!");
  return has_error() ? *Error : EmptyError;
}

void parser::set_error_to_current_token(std::string Reason) noexcept
{
  if (Error && Error->TokenKind == token::SyntaxError)
    return;
  set_error(prepare_error(std::move(Reason)));
}

void parser::set_error(error_info&& Err, std::string Reason) noexcept
{
  if (!Reason.empty())
    Err.Reason = std::move(Reason);
  Error.emplace(std::move(Err));
}

parser::error_info parser::prepare_error(std::string Reason) noexcept
{
  return error_info{.Location = Lexer.get_location(),
                    .TokenKind = get_current_token(),
                    .TokenInfo = Lexer.get_token_info_raw(),
                    .Reason = std::move(Reason)};
}

token parser::get_current_token() noexcept
{
  MONOMUX_DEBUG(std::cout << "< ### Access...> ");
  token T = Lexer.current_token();
  MONOMUX_DEBUG(std::cout << Lexer.get_location().Line << ':'
                          << Lexer.get_location().Column << ' ';
                std::cout << to_string(Lexer.get_token_info_raw())
                          << std::endl;);
  return T;
}

token parser::get_next_token() noexcept
{
  MONOMUX_DEBUG(std::cout << "< ---> Lex...> ");
  token T = Lexer.lex();
  if (T == token::SyntaxError)
  {
    Error.emplace(error_info{
      .Location = Lexer.get_location(),
      .TokenKind = T,
      .TokenInfo = Lexer.get_token_info_raw(),
      .Reason = Lexer.get_token_info<token::SyntaxError>()->to_string()});
  }

  MONOMUX_DEBUG(std::cout << Lexer.get_location().Line << ':'
                          << Lexer.get_location().Column << ' ';
                std::cout << to_string(Lexer.get_token_info_raw())
                          << std::endl;);

  return T;
}

token parser::peek_next_token() noexcept
{
  MONOMUX_DEBUG(std::cout << "< ---> Peek...> ");
  token T = Lexer.peek();
  MONOMUX_DEBUG(std::cout << Lexer.get_location().Line << ':'
                          << Lexer.get_location().Column << ' ';
                std::cout << to_string(Lexer.get_token_info_raw())
                          << std::endl;);
  return T;
}

} // namespace monomux::tools::dto_compiler
