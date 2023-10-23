Serialisation system compiler
=============================

> Requirement-specifier phrases, such as **_MUST_**, **_RECOMMENDED_**, **_MAY_**, **_SHOULD NOT_**, etc., in every context where written as such (i.e., capitalised, **bold** and _italic_) **_MUST_** be understood as per [_RFC 2119_](http://datatracker.ietf.org/doc/html/rfc2119).

`dto_compiler` implements a precompiler for the communication protocol used by Monomux's server-client architecture.
The real main source file used for this purpose is available at: [`src/implementation/Monomux.dto`](/src/implementation/Monomux.dto).

`dto_compiler` translates the communication layer's DTO DSL into C++ source code that is then embedded into the built binaries.
The created output code is created as a self-contained system that is only extended by the client code's adding of callback behaviours, so there is no need to link additional libraries.

Format of the DTO DSL
---------------------

The communication protocol's specification must be made available as a **single** text file, commonly with the `.dto` extension.
As a rule of thumb, the usual semantics and guidelines of C++ translation apply: things should be defined top to bottom, i.e., before using an entity, its definition **_MUST_** already have been made available.

### Identifiers

Any character sequence that is not matching to an explicitly defined meaning in this document is an _identifier_.
Identifiers carry no meaning during compilation, and are translated verbatim into the generated output code.

### Comments

```
// This comment is removed.
/* This comment
   is removed.
               */

//! This comment is retained. Good for documentation!
Entity();

/*! This comment is retained.
    Description of the entity...
    Inputs:
      - A
      - B
    Outputs:
      - C
      - D
*/
Entity();
```

#### Stripped comments 

Comments follow the usual C++ syntax, i.e., `// Comment.` for single-line comments (everything following the `//` until the next newline is considered to be a comment) and `/* Comment. */` for potentially multi-line comments.

These comments are stripped during compilation.

Unlike in C++, multi-line comments in the DTO **_MAY_** embed additional "blocks" of multi-line comments — the parsing of the comment does not end at the first closing `*/`.
In this case, the number of opening `/*` and closing `*/` **_MUST_** balance out.

#### Retained comments

As the compiler's goal is to generate source code which might be read by fellow developers, there can be comments embedded into the source file that are **retained** during compilation and added to the output verbatim.
The comment is present in the output at the location it was originally present in the input source file, in the order of declarations.

A **`!`** **_MUST_** immediately follow the — first, if there are multiple — comment-opening token `//` or `/*`, i.e., appear in the form of `//!` and `/*!`.
Apart from not getting stripped from the generated output, the rest of the semantics of [Stripped comments](#stripped-comments) apply.

### Namespaces

```
namespace MyProgram::Communication
{

  /* entities ... */

}
```

Entities in the DTO DSL and the resulting C++ source code may be enclosed into **`namespace`**s.
The semantics adhere to how the language feature works in C++.

A namespace's name **_MUST_** be a pure identifier, or a sequence of identifiers concatenated by the "scope operator" `::`.
A direct line of `namespace`s can be declared in the shorthand form, which is equivalent to subsequent `namespace` declarations: `namespace A::B::C { ... }` is the same as `namespace A { namespace B { namespace C { ... }}}`.

A `namespace` **_MAY_** be opened multiple times, i.e. `namespace A { ... } ... namespace A { ... }`.
In this case, the elements of the visually two "blocks" belong to the same `namespace`.

#### Name resolution order

When a use of a named entity — type, variable, or record — is encountered, name resolution is performed from the innermost namespace — where the use was present — outwards.
The first matching name is the result of the resolution, and no further checks are performed.

It is **_NOT RECOMMENDED_** to reuse the same identifier for multiple declarations, despite those declarations added to unique `namespace`s.

### Types

The following types are available.

 - Integer types of fixed bit width: `ui64`.

#### Record types

### Literals

Integer literals are expressed as a sequence of decimal characters `0`, `1`, `2`, …, `9`.
Integer literals can be negative, in case they are prefixed with a `-`.

These expressions appear as the initialising values of constants.

### Constants

```
literal ui64 Version = 1;
```

Constants are introduced with the **`literal`** keyword, followed by the [type](#types) of the constant, followed by its [identifier (name)](#identifiers) and an [initialising value](#literals).
Constants are translated to the generated code as compile-time constant objects.

### Functions
