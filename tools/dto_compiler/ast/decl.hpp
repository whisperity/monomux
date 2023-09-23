/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"

#include "expr.hpp"
#include "type.hpp"

namespace monomux::tools::dto_compiler::ast
{

class comment
{
  bool IsBlockComment;
  std::string Comment;

public:
  comment(bool BlockComment, std::string Comment)
    : IsBlockComment(BlockComment), Comment(std::move(Comment))
  {}

  [[nodiscard]] bool is_block_comment() const noexcept
  {
    return IsBlockComment;
  }
  [[nodiscard]] const std::string& get_comment() const noexcept
  {
    return Comment;
  }
};

class decl
{

public:
  MONOMUX_MAKE_STRICT_TYPE(decl, virtual);
  virtual void dump(std::ostringstream& OS, std::size_t Depth) const;
};

#define MONOMUX_DECL_DUMP                                                      \
  void dump(std::ostringstream& OS, std::size_t Depth) const override;

/// A faux node of supertype \p decl that only holds a comment.
class comment_decl : public decl
{
  comment Comment;

public:
  explicit comment_decl(comment C) : Comment(std::move(C)) {}
  MONOMUX_DECL_DUMP;
};

class named_decl : public decl
{
  std::string Identifier;

public:
  explicit named_decl(std::string Identifier)
    : Identifier(std::move(Identifier))
  {}

  [[nodiscard]] const std::string& get_identifier() const noexcept
  {
    return Identifier;
  }

  MONOMUX_DECL_DUMP;
};

/// Represents a kind of \p decl that may store inner child \p decl nodes.
class decl_context
{
  std::vector<std::unique_ptr<decl>> Children;
  std::unordered_map<std::string, named_decl*> NameableChildren;

public:
  MONOMUX_MAKE_STRICT_TYPE(decl_context, );
  void dump_children(std::ostringstream& OS, std::size_t Depth = 0) const;

  template <typename DeclType, typename... Args>
  DeclType* get_or_create_child_decl(Args&&... Argv)
  {
    std::unique_ptr<decl> Node =
      std::make_unique<DeclType>(std::forward<Args>(Argv)...);

    if constexpr (std::is_base_of_v<named_decl, DeclType>)
    {
      if (auto It = NameableChildren.find(
            dynamic_cast<named_decl&>(*Node).get_identifier());
          It != NameableChildren.end())
        return dynamic_cast<DeclType*>(It->second);
    }

    Children.push_back(std::move(Node));
    auto* OwnedNode = static_cast<DeclType*>(Children.back().get());

    if constexpr (std::is_base_of_v<named_decl, DeclType>)
    {
      const auto* ND = dynamic_cast<named_decl*>(OwnedNode);
      NameableChildren[ND->get_identifier()] = OwnedNode;
    }

    return OwnedNode;
  }

  const decl* lookup(const std::string& Identifier) const noexcept
  {
    auto It = NameableChildren.find(Identifier);
    if (It == NameableChildren.end())
      return nullptr;

    return It->second;
  }
};

class namespace_decl
  : public named_decl
  , public decl_context
{
public:
  explicit namespace_decl(std::string Identifier)
    : named_decl(std::move(Identifier))
  {}
  MONOMUX_DECL_DUMP;
};

/// Declarations that have associated types.
class value_decl : public named_decl
{
  type* Type;

public:
  explicit value_decl(std::string Identifier, type* Type)
    : named_decl(std::move(Identifier)), Type(Type)
  {}

  [[nodiscard]] const type* get_type() const noexcept { return Type; }

  MONOMUX_DECL_DUMP;
};

class literal_decl : public value_decl
{
  expr* Value;

public:
  explicit literal_decl(std::string Identifier, type* Type, expr* Value)
    : value_decl(std::move(Identifier), Type), Value(Value)
  {}

  [[nodiscard]] const expr* get_value() const noexcept { return Value; }

  MONOMUX_DECL_DUMP;
};

#undef MONOMUX_DECL_DUMP

} // namespace monomux::tools::dto_compiler::ast
