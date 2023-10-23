/* SPDX-License-Identifier: LGPL-3.0-only */
#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
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

namespace detail
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

} // namespace detail

class decl
{
public:
  MONOMUX_MAKE_STRICT_TYPE(decl, virtual);
  void dump(std::size_t Depth = 0) const;
  virtual void dump(std::ostringstream& OS, std::size_t Depth) const;
};

#define MONOMUX_DECL_DUMP                                                      \
  void dump(std::ostringstream& OS, std::size_t Depth) const override;

/// A faux node of supertype \p decl that only holds a comment.
class comment_decl : public decl
{
  detail::comment Comment;

public:
  MONOMUX_DECL_DUMP;
  explicit comment_decl(detail::comment C) : Comment(std::move(C)) {}

  [[nodiscard]] const detail::comment& get_comment_info() const noexcept
  {
    return Comment;
  }
};

class named_decl : public decl
{
  std::string Identifier;

protected:
  explicit named_decl(std::string Identifier)
    : Identifier(std::move(Identifier))
  {}

public:
  MONOMUX_DECL_DUMP;

  [[nodiscard]] const std::string& get_identifier() const noexcept
  {
    return Identifier;
  }
};

class type_decl : public named_decl
{
  type* Type;

public:
  MONOMUX_DECL_DUMP;
  type_decl(std::string Identifier, type* Type)
    : named_decl(std::move(Identifier)), Type(Type)
  {}

  [[nodiscard]] const type* get_type() const noexcept { return Type; }
};

/// Represents a kind of \p decl that may store inner child \p decl nodes.
class decl_context
{
  std::vector<std::unique_ptr<decl>> Children;
  decl_context* Previous{};
  decl_context* Next{};

  decl_context* Parent{};
  void set_parent(decl_context* DC) noexcept { Parent = DC; }

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
        *Iterator = iterator_type{*Iterator->Container->next()};
    }
  };

public:
  using iterator = iterator_type<false>;
  using const_iterator = iterator_type<true>;

  MONOMUX_MAKE_STRICT_TYPE(decl_context, virtual);

  /// Dumps the \p Children of the \b current instance (but not the chained
  /// instances), by recursively calling \p decl::dump() on them.
  void dump_children(std::size_t Depth = 0) const;
  void dump_children(std::ostringstream& OS, std::size_t Depth = 0) const;

  [[nodiscard]] iterator begin() noexcept { return iterator{first_in_chain()}; }
  [[nodiscard]] const_iterator begin() const noexcept
  {
    return const_iterator{first_in_chain()};
  }
  [[nodiscard]] iterator end() noexcept
  {
    return iterator{last_in_chain(), iterator::end_iterator_tag{}};
  }
  [[nodiscard]] const_iterator end() const noexcept
  {
    return const_iterator{last_in_chain(), const_iterator::end_iterator_tag{}};
  }

  [[nodiscard]] decl_iterator<false> children_begin() noexcept
  {
    return decl_iterator<false>(&*Children.begin());
  }
  [[nodiscard]] decl_iterator<true> children_begin() const noexcept
  {
    return decl_iterator<true>(&*Children.begin());
  }
  [[nodiscard]] decl_iterator<false> children_end() noexcept
  {
    return decl_iterator<false>(&*Children.end());
  }
  [[nodiscard]] decl_iterator<true> children_end() const noexcept
  {
    return decl_iterator<true>(&*Children.end());
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

  [[nodiscard]] const decl_context* parent() const noexcept { return Parent; }
  MONOMUX_MEMBER_0(decl_context*, parent, [[nodiscard]], noexcept);

  template <typename DeclTy, typename... Args>
  DeclTy* emplace_child(Args&&... Argv)
  {
    Children.emplace_back(
      std::make_unique<DeclTy>(std::forward<Args>(Argv)...));
    auto* OwnedNode = static_cast<DeclTy*>(Children.back().get());

    if constexpr (std::is_base_of_v<decl_context, DeclTy>)
      static_cast<decl_context*>(OwnedNode)->set_parent(this);

    return OwnedNode;
  }

  /// \p emplace_child() in the current context's full \e chain, but the
  /// insertion is performed to the location in the \p Children vector before
  /// the specified \p Node. If the \p Node is not found, the child is inserted
  /// to the \b beginning of the list of children for the context.
  ///
  /// Inserting before a \p nullptr is equivalent to inserting to the very
  /// beginning of the children list.
  template <typename DeclTy, typename... Args>
  DeclTy* emplace_child_in_chain_before(const decl* Node, Args&&... Argv)
  {
    decl_context* DC = &first_in_chain();
    auto FoundNodeIt = DC->Children.begin();
    if (Node)
    {
      for (; DC; DC = DC->next())
      {
        auto NodeIt = std::find_if(
          DC->Children.begin(),
          DC->Children.end(),
          [Node](const auto& NodeUPtr) { return NodeUPtr.get() == Node; });
        if (NodeIt != DC->Children.end())
        {
          FoundNodeIt = NodeIt;
          break;
        }
      }

      if (!DC)
      {
        assert(FoundNodeIt == first_in_chain().Children.begin());
        DC = &first_in_chain();
      }
    }

    auto InsertIt = DC->Children.insert(
      FoundNodeIt, std::make_unique<DeclTy>(std::forward<Args>(Argv)...));
    auto* OwnedNode = static_cast<DeclTy*>(InsertIt->get());

    if constexpr (std::is_base_of_v<decl_context, DeclTy>)
      static_cast<decl_context*>(OwnedNode)->set_parent(this);

    return OwnedNode;
  }

  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in all contexts that are part of the \e chain the
  /// current context is part of.
  [[nodiscard]] const named_decl*
  lookup(std::string_view Identifier) const noexcept;
  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in all contexts that are part of the \e chain the
  /// current context is part of.
  MONOMUX_MEMBER_1(
    named_decl*, lookup, [[nodiscard]], noexcept, std::string_view, Identifier);

  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in the \b current context.
  [[nodiscard]] const named_decl*
  lookup_in_current(std::string_view Identifier) const noexcept;
  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in the \b current context.
  MONOMUX_MEMBER_1(named_decl*,
                   lookup_in_current,
                   [[nodiscard]],
                   noexcept,
                   std::string_view,
                   Identifier);

  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in all contexts that are part of the \e chain the
  /// current context is part of, just like \p lookup().
  ///
  /// If unsuccessful, the search begins in the \p parent() context of the
  /// current chain, if such exists.
  [[nodiscard]] const named_decl*
  lookup_with_parents(std::string_view Identifier) const noexcept;
  /// Attempts to find, and if successful, returns the \p named_decl with the
  /// given \p Identifier in all contexts that are part of the \e chain the
  /// current context is part of, just like \p lookup().
  ///
  /// If unsuccessful, the search begins in the \p parent() context of the
  /// current chain, if such exists.
  MONOMUX_MEMBER_1(named_decl*,
                   lookup_with_parents,
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
  MONOMUX_DECL_DUMP;
  explicit namespace_decl(std::string Identifier)
    : named_decl(std::move(Identifier))
  {
    assert(get_identifier().find(':') == std::string::npos &&
           "Nested namespaces should not be represented as a single object!");
  }

  /// Returns the outermost \p namespace_decl that does not have any more
  /// \p namespace_decl parents. This is \b NEVER the "global namespace" which
  /// is, in fact, not represented as a \p namespace_decl.
  [[nodiscard]] const namespace_decl* get_outermost_namespace() const noexcept;
  MONOMUX_MEMBER_0(namespace_decl*,
                   get_outermost_namespace,
                   [[nodiscard]],
                   noexcept);
};

/// Declarations that have associated types.
class value_decl : public named_decl
{
  const type* Type;

public:
  MONOMUX_DECL_DUMP;
  explicit value_decl(std::string Identifier, const type* Type)
    : named_decl(std::move(Identifier)), Type(Type)
  {}

  [[nodiscard]] const type* get_type() const noexcept { return Type; }
};

class literal_decl : public value_decl
{
  const expr* Value;

public:
  MONOMUX_DECL_DUMP;
  explicit literal_decl(std::string Identifier,
                        const type* Type,
                        const expr* Value)
    : value_decl(std::move(Identifier), Type), Value(Value)
  {}

  [[nodiscard]] const expr* get_value() const noexcept { return Value; }
};

#undef MONOMUX_DECL_DUMP

} // namespace monomux::tools::dto_compiler::ast
