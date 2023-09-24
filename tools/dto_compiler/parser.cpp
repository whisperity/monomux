/* SPDX-License-Identifier: LGPL-3.0-only */
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif /* !NDEBUG */

#include "monomux/Debug.h"
#include "monomux/adt/scope_guard.hpp"

#include "ast/decl.hpp"
#include "dto_unit.hpp"
#include "lexer.hpp"

#include "parser.hpp"

namespace monomux::tools::dto_compiler
{

namespace
{

const parser::error_info EmptyError{};

} // namespace

#define INFO(TOKEN_KIND)                                                       \
  const auto& Info = Lexer.get_token_info<token::TOKEN_KIND>();

std::vector<std::string> parser::parse_potentially_scoped_identifier()
{
  std::vector<std::string> Identifiers;
  Identifiers.emplace_back("");
  while (true)
  {
    token T = get_current_token();
    if (T == token::Scope)
      Identifiers.emplace_back("");
    else if (T == token::Identifier)
    {
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
  // First, need to consume the identifier of the namespace.
  assert(get_current_token() == token::Namespace && "Expected 'namespace'");
  (void)get_next_token();
  std::vector<std::string> Identifiers = parse_potentially_scoped_identifier();

  if (get_current_token() != token::LBrace)
    set_error_to_current_token("Expected '{' after namespace identifier "
                               "declaration");

  restore_guard G{DeclContext};
  auto* NSD = [this,
               &Identifiers,
               Context = this->DeclContext]() mutable -> ast::namespace_decl* {
    for (std::string NextScope : Identifiers)
    {
      ast::decl* ExistingD = Context->lookup_in_current(NextScope);
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
        set_error_to_current_token(std::string{"Unexpected "} +
                                   std::string{to_string(get_current_token())} +
                                   std::string{" encountered while parsing."});
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
          ast::comment{Info->IsBlockComment, Info->Comment});
        break;
      }

      case token::Namespace:
        parse_namespace();
        break;

      case token::Literal:
      case token::Function:
      case token::Record:
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
  Error.emplace(error_info{.Location = Lexer.get_location(),
                           .TokenKind = get_current_token(),
                           .TokenInfo = Lexer.get_token_info_raw(),
                           .Reason = std::move(Reason)});
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

} // namespace monomux::tools::dto_compiler
