/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <cassert>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "monomux/adt/FunctionExtras.hpp"
#include "monomux/adt/bucket_iterator.hpp"

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
  decl_context* Previous{};
  decl_context* Next{};

  /// Iterates over the \p decls owned in the \p Children collection,
  /// transparently bypassing the ownership data structures.
  template <bool Const> struct decl_iterator
  {
    using collection_iterator_type =
      typename std::conditional_t<Const,
                                  decltype(Children)::iterator,
                                  decltype(Children)::const_iterator>;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = typename collection_iterator_type::difference_type;
    using value_type = std::conditional_t<Const, const decl, decl>;
    using pointer = value_type*;
    using reference = value_type&;

    using element_type = typename std::conditional_t<
      Const,
      const typename collection_iterator_type::value_type,
      typename collection_iterator_type::value_type>;
    element_type* Ptr;

    explicit decl_iterator(element_type* E) : Ptr(E) {}

    [[nodiscard]] bool operator==(const decl_iterator& RHS) const noexcept
    {
      return Ptr == RHS.Ptr;
    }
    [[nodiscard]] bool operator!=(const decl_iterator& RHS) const noexcept
    {
      return Ptr != RHS.Ptr;
    }

    decl_iterator& operator++() noexcept
    {
      ++Ptr;
      return *this;
    }
    [[nodiscard]] decl_iterator operator++(int) noexcept
    {
      auto Tmp = *this;
      ++(*this);
      return Tmp;
    }

    [[nodiscard]] reference operator*() const noexcept { return **Ptr; }
    [[nodiscard]] pointer operator->() const noexcept { return &**Ptr; }
  };

  /// Iterates over the \p Children of the current \p decl_context and all
  /// additional \p decl_context instances chained together with the the
  /// current one.
  template <bool Const>
  class iterator_type
    : public forward_bucket_iterator_adaptor<iterator_type<Const>,
                                             decl_iterator<Const>>
  {
  private:
    using base = forward_bucket_iterator_adaptor<iterator_type<Const>,
                                                 decl_iterator<Const>>;
    using collection_type =
      std::conditional_t<Const, const decl_context, decl_context>;
    collection_type* Container;

  public:
    struct end_iterator_tag
    {};

    explicit iterator_type(collection_type& C)
      : base{decl_iterator<Const>{C.Children.data()},
             decl_iterator<Const>{C.Children.data() + C.Children.size()}},
        Container(&C)
    {}

    iterator_type(collection_type& C, end_iterator_tag&& /*Tag*/)
      : base{decl_iterator<Const>{C.Children.data() + C.Children.size()},
             decl_iterator<Const>{C.Children.data() + C.Children.size()}},
        Container(&C)
    {}

    static void set_next_bucket(iterator_type* Iterator) noexcept
    {
      if (!Iterator->Container->next())
        // If the current container is the last in the chain, stay put and
        // signal that we reached the end of the range.
        *Iterator = Iterator->Container->end();
      else
        // Otherwise, start iterating the next "bucket" in the chain.
        *Iterator = Iterator->Container->next()->local_begin();
    }
  };

public:
  using iterator = iterator_type<false>;
  using const_iterator = iterator_type<true>;

  MONOMUX_MAKE_STRICT_TYPE(decl_context, virtual);

  /// Dumps the \p Children of the \b current instance (but not the chained
  /// instances), by recursively calling \p decl::dump() on them.
  void dump_children(std::ostringstream& OS, std::size_t Depth = 0) const;

  [[nodiscard]] iterator begin() noexcept
  {
    return first_in_chain().local_begin();
  }
  [[nodiscard]] const_iterator begin() const noexcept
  {
    return first_in_chain().local_begin();
  }
  [[nodiscard]] iterator local_begin() noexcept { return iterator{*this}; }
  [[nodiscard]] const_iterator local_begin() const noexcept
  {
    return const_iterator{*this};
  }

  [[nodiscard]] iterator_type<false> end() noexcept
  {
    return last_in_chain().local_end();
  }
  [[nodiscard]] iterator_type<true> end() const noexcept
  {
    return last_in_chain().local_end();
  }
  [[nodiscard]] iterator local_end() noexcept
  {
    return iterator{*this, iterator::end_iterator_tag{}};
  }
  [[nodiscard]] const_iterator local_end() const noexcept
  {
    return const_iterator{*this, const_iterator::end_iterator_tag{}};
  }

  [[nodiscard]] const decl_context* prev() const noexcept { return Previous; }
  [[nodiscard]] const decl_context* next() const noexcept { return Next; }
  MONOMUX_MEMBER_0(decl_context*, prev, [[nodiscard]], noexcept);
  MONOMUX_MEMBER_0(decl_context*, next, [[nodiscard]], noexcept);

  void add_to_chain(decl_context& NextDC) noexcept
  {
    assert(!Next &&
           "Adding non-linear chains breaks object, add to last instea!");
    Next = &NextDC;
    NextDC.Previous = this;
  }

  [[nodiscard]] const decl_context& first_in_chain() const noexcept
  {
    if (prev())
      return prev()->first_in_chain();
    return *this;
  }
  [[nodiscard]] const decl_context& last_in_chain() const noexcept
  {
    if (next())
      return next()->last_in_chain();
    return *this;
  }
  MONOMUX_MEMBER_0(decl_context&, first_in_chain, [[nodiscard]], noexcept);
  MONOMUX_MEMBER_0(decl_context&, last_in_chain, [[nodiscard]], noexcept);

  template <typename DeclTy, typename... Args>
  DeclTy* emplace_child(Args&&... Argv)
  {
    Children.emplace_back(
      std::make_unique<DeclTy>(std::forward<Args>(Argv)...));
    auto* OwnedNode = static_cast<DeclTy*>(Children.back().get());
    return OwnedNode;
  }

  [[nodiscard]] const decl* lookup(std::string_view Identifier) const noexcept;
  MONOMUX_MEMBER_1(
    decl*, lookup, [[nodiscard]], noexcept, std::string_view, Identifier);

  [[nodiscard]] const decl*
  lookup_in_current(std::string_view Identifier) const noexcept;
  MONOMUX_MEMBER_1(decl*,
                   lookup_in_current,
                   [[nodiscard]],
                   noexcept,
                   std::string_view,
                   Identifier);
};

class namespace_decl
  : public named_decl
  , public decl_context
{
public:
  explicit namespace_decl(std::string Identifier)
    : named_decl(std::move(Identifier))
  {
    assert(get_identifier().find(':') == std::string::npos &&
           "Nested namespaces should not be represented as a single object!");
  }

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
