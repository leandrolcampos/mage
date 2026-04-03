# Mage Coding Standards

## Introduction

This document describes the coding standards used by the Mage project.

Mage follows standard C++17 and adopts a style inspired by the LLVM Coding Standards, adapted to the current needs of this repository. Although no coding standard should be regarded as an absolute requirement to be followed in all instances, coding standards are particularly important for large-scale code bases that follow a library-based design.

While this document provides guidance for formatting, whitespace, and other details, these are not fixed standards in all situations. Always follow the golden rule:

> **If you are extending, enhancing, or bug fixing already implemented code, use the style that is already being used so that the source is uniform and easy to follow.**

Note that some code bases have special reasons to deviate from the coding standards.

There are some conventions that may not yet be uniformly followed throughout the code base. Our long-term goal is for the entire code base to follow the conventions in this document, but we explicitly do not want patches that do large-scale reformatting of existing code. On the other hand, it is reasonable to rename methods or adjust local style when you are already changing the surrounding code for another reason. Keep such cleanup separate from functional changes whenever possible to make review easier.

The ultimate goal of these guidelines is to increase the readability and maintainability of the Mage source base.

## Languages, Libraries, and Standards

### C++ Language

Mage is written in standard C++17. Avoid unnecessary vendor-specific extensions.

Use language and library features that are available in the host toolchain used to build Mage. Mage is expected to be built with the Clang toolchain described in [`BuildingLLVM.md`](BuildingLLVM.md).

### C++ Standard Library

Mage uses the C++ standard library provided by the host toolchain and system environment.

Instead of implementing custom data structures, prefer the C++ standard library or LLVM support libraries whenever they already provide suitable facilities for the task.

When both the C++ standard library and LLVM support libraries provide similar functionality, and there is no specific reason to favor the C++ implementation, it is generally preferable to use the LLVM facility in LLVM-centric code. For example, `llvm::DenseMap` will often be a better fit than `std::unordered_map`, and `llvm::SmallVector` will often be a better fit than `std::vector`.

We explicitly avoid some standard facilities, like the I/O streams, and instead use LLVM's stream library (`raw_ostream`) where appropriate.

## Mechanical Source Issues

### Source Code Formatting

#### Commenting

Comments are important for readability and maintainability. When writing comments, write them as English prose, using proper capitalization and punctuation. Aim to describe what the code is trying to do and why, not how it does it at a micro level.

Here are a few important things to document:

- the purpose of a source file;
- the purpose of a class and how it should be used;
- the purpose and behavior of public functions;
- non-obvious assumptions, invariants, preconditions, postconditions, and edge cases.

##### File Headers

Every source file should have a header on it that describes the basic purpose of the file. The standard header looks like this:

```cpp
//===----------------------------------------------------------------------===//
//
// Part of the Mage project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains ...
///
//===----------------------------------------------------------------------===//
```

The first section in the file is a concise note that defines the license that the file is released under. This makes it perfectly clear what terms the source code can be distributed under and should not be modified in any way.

The main body is a Doxygen comment, identified by the `///` marker, describing the purpose of the file. The first sentence, or a paragraph beginning with `\brief`, is used as an abstract. Any additional information should be separated by a blank line. If an algorithm is based on a paper or is described in another source, provide a reference.

##### Header Guards

Header guards should use the all-caps path that a user of the header would `#include`, using `_` instead of path separators and the extension marker.

For example, the header file `include/mage/Support/MathExtras.hpp` would be `#include`-ed as:

```cpp
#include "mage/Support/MathExtras.hpp"
```

so its guard should be:

```cpp
MAGE_SUPPORT_MATHEXTRAS_HPP
```

##### Class Overviews

Classes are a fundamental part of an object-oriented design. As such, a class definition should have a comment block that explains what the class is used for and how it works. Every non-trivial class is expected to have a Doxygen comment block.

##### Method Information

Methods and global functions should also be documented when the documentation adds useful information. A quick note about what the function does and a description of edge cases is often enough. The reader should be able to understand how to use interfaces without reading the implementation.

Good things to document are what happens when something unexpected happens, whether a function can return `nullptr`, whether ownership is transferred, and what invariants the caller must respect.

#### Comment Formatting

In general, prefer C++-style comments (`//` for normal comments and `///` for Doxygen documentation comments). There are a few cases when it is useful to use C-style (`/* */`) comments, however:

1. When writing C code to be compatible with C89.
2. When writing a header file that may be `#include`-d by a C source file.
3. When writing a source file that is used by a tool that only accepts C-style comments.
4. When documenting the significance of constants used as actual parameters in a call. This is most helpful for `bool` parameters, or when passing `0` or `nullptr`. The comment should contain the parameter name, which ought to be meaningful.

For example, this is not very clear:

```cpp
Object.emitName(nullptr);
```

An inline C-style comment makes the intent obvious:

```cpp
Object.emitName(/*Prefix=*/nullptr);
```

Commenting out large blocks of code is discouraged, but if you really have to do this, use `#if 0` and `#endif`. These nest properly and are better behaved in general than C-style comments.

#### Doxygen Use in Documentation Comments

Use the `\file` command to turn the standard file header into a file-level comment.

Include descriptive paragraphs for all public interfaces: public classes, public member functions, and public non-member functions. Avoid restating information that can be inferred from the API name or signature. The first sentence, or a paragraph beginning with `\brief`, is used as an abstract. Try to use a single sentence when possible.

A minimal documentation comment:

```cpp
/// Sets the xyzzy property to \p Baz.
void setXyzzy(bool Baz);
```

Only include code examples, function parameters, and return values when doing so provides additional information, such as intent, usage, or behavior that is non-obvious. Use descriptive function and argument names to eliminate the need for documentation comments when possible.

To refer to parameter names inside a paragraph, use the `\p name` command. Do not use `\arg name`, since it starts a new paragraph.

Wrap non-inline code examples in `\code ... \endcode`.

To document a function parameter, start a new paragraph with `\param name`. If the parameter is used as an out or an in/out parameter, use `\param [out] name` or `\param [in,out] name`, respectively.

To describe a function return value, start a new paragraph with `\returns`.

A documentation comment that uses all Doxygen features in a preferred way:

```cpp
/// Does foo and bar.
///
/// Does not do foo the usual way if \p Baz is true.
///
/// Typical usage:
/// \code
///   fooBar(false, "quux", Result);
/// \endcode
///
/// \param Quux Kind of foo to do.
/// \param [out] Result Filled with bar sequence on success.
///
/// \returns True on success.
bool fooBar(bool Baz, llvm::StringRef Quux, std::vector<int> &Result);
```

Do not duplicate the documentation comment in the header file and the implementation file. Put documentation comments for public APIs in the header file. Documentation comments for private APIs can go in the implementation file. Implementation files can also include additional comments to explain implementation details as needed.

Do not duplicate the function or class name at the beginning of the comment.

Avoid:

```cpp
// Example.hpp:

// example - Does something important.
void example();
```

Preferred:

```cpp
// Example.hpp:

/// Does something important.
void example();
```

#### Error and Warning Messages

Clear diagnostic messages are important to help users identify and fix issues in their inputs. Use succinct but correct English prose that gives the user the context needed to understand what went wrong. To match styles commonly produced by other tools, start the first sentence with a lowercase letter and finish the last sentence without a period if it would end in one otherwise. Sentences that end with different punctuation, such as `did you forget ';'?`, should still do so.

For example, this is a good error message:

```text
error: file.o: section header 3 is corrupt: size is 10 when it should be 20
```

This is a bad message:

```text
error: file.o: corrupt section header.
```

If a Mage component already has an established diagnostic style that is used consistently throughout that component, follow that style. Otherwise, use the style described above.

If Mage code needs to emit diagnostics, use the project's established diagnostic mechanism when available instead of printing ad hoc messages directly to `stderr`.

Do not depend on host-only diagnostic facilities in code that must also support device builds.

#### `#include` Style

Immediately after the file header comment, and include guards if working on a header file, the minimal list of `#include`s required by the file should be listed. We prefer these `#include`s to be listed in this order:

1. Main module header.
2. Local/private headers.
3. Mage project headers.
4. External dependency headers, including LLVM headers.
5. System `#include`s.

Within each category, sort includes lexicographically by full path.

The main module header applies to `.cpp` files that implement an interface defined by a public header. This `#include` should always be included first regardless of where it lives on the file system. By including the module header first in the `.cpp` file that implements it, we ensure that the header does not have hidden dependencies that should instead be included explicitly by the header itself. It is also a form of documentation in the `.cpp` file.

Mage headers should be grouped before LLVM headers because Mage is the project being implemented, while LLVM is an external dependency. LLVM headers should be grouped before system headers for the same reason that project headers are grouped before system headers in LLVM: this reduces the chance that a project header accidentally relies on a transitive include from a system header.

For example:

```cpp
#include "mage/Support/MathExtras.hpp"

#include "MathExtrasInternal.hpp"

#include "mage/Support/FPBits.hpp"
#include "mage/Support/TypeTraits.hpp"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cstdint>
#include <utility>
```

If a file does not have a corresponding public header, start with the first applicable category above.

#### Source Code Width

Write your code to fit within 80 columns.

#### Whitespace

In all cases, prefer spaces to tabs in source files.

Do not add trailing whitespace.

As always, follow the golden rule: follow the style of existing code if you are modifying and extending it.

##### Format Lambdas Like Blocks of Code

When formatting a multi-line lambda, format it like a block of code. If there is only one multi-line lambda in a statement, and there are no expressions lexically after it in the statement, drop the indent to the standard two-space indent for a block of code, as if it were an `if` block opened by the preceding part of the statement:

```cpp
llvm::sort(Foo.begin(), Foo.end(), [&](const Node &LHS, const Node &RHS) -> bool {
  if (LHS.Priority < RHS.Priority)
    return true;
  if (LHS.Priority > RHS.Priority)
    return false;
  return LHS.Name < RHS.Name;
});
```

If there are multiple multi-line lambdas in a statement, or additional parameters after the lambda, indent the block two spaces from the indent of the `[]`:

```cpp
dispatch(Node,
         [](BinaryNode *N) {
           // process binary node...
         },
         [](UnaryNode *N) {
           // process unary node...
         });
```

If you are designing an API that accepts a continuation or single callable argument, it should be the last argument when practical.

##### Braced Initializer Lists

Starting from C++11, there are significantly more uses of braced lists to perform initialization. We suggest new code use a simple rule for formatting braced initialization lists: act as if the braces were parentheses in a function call.

Examples:

```cpp
foo({A, B, C}, {1, 2, 3});

llvm::StringRef Names[] = {
    "Alpha",
    "Beta",
    "Gamma"};
```

If you use a braced initializer list when initializing a variable, use an equals sign before the open curly brace:

```cpp
int Data[] = {0, 1, 2, 3};
```

### Language and Compiler Issues

#### Treat Compiler Warnings Like Errors

Compiler warnings are often useful and help improve the code. Those that are not useful can often be suppressed with a small code change. For example, an assignment in an `if` condition is often a typo:

```cpp
if (Value = getValue()) {
  // ...
}
```

Several compilers will warn on code like this. It can be suppressed by adding parentheses:

```cpp
if ((Value = getValue())) {
  // ...
}
```

#### Write Portable Code

In almost all cases, it is possible to write completely portable code. When you need to rely on non-portable code, put it behind a well-defined and well-documented interface.

#### Do Not Use RTTI or Exceptions

Mage does not use C++ exceptions or RTTI in normal project code. Avoid language features such as `dynamic_cast<>` and `typeid` for runtime type identification.

Use LLVM-style facilities such as `isa<>`, `cast<>`, and `dyn_cast<>` where appropriate.

#### Prefer C++-Style Casts

When casting, use `static_cast`, `reinterpret_cast`, and `const_cast`, rather than C-style casts. There are two exceptions to this:

- When casting to `void` to suppress warnings about unused variables.
- When casting between integral types, including non-strongly-typed enums, functional-style casts are permitted as an alternative to `static_cast`.

#### Do Not Use Static Constructors

Static constructors and destructors, such as global variables whose types have a constructor or destructor, should not be added to the code base and should be removed wherever possible.

Globals in different source files are initialized in an arbitrary order, making the code more difficult to reason about.

Avoid:

```cpp
SomeRegistry GlobalRegistry;
```

Prefer explicit initialization or function-local state when appropriate.

#### Use of `class` and `struct` Keywords

In C++, the `class` and `struct` keywords can be used almost interchangeably. The only difference is that `class` makes all members private by default while `struct` makes all members public by default.

All declarations and definitions of a given `class` or `struct` must use the same keyword.

`struct` should be used when all members are declared public.

Avoid:

```cpp
struct Foo {
private:
  int Data;
public:
  Foo() : Data(0) {}
};
```

Preferred:

```cpp
class Foo {
  int Data;
public:
  Foo() : Data(0) {}
};
```

#### Do Not Use Braced Initializer Lists to Call a Constructor

Starting from C++11, there is a generalized initialization syntax that allows calling constructors using braced initializer lists. Do not use braced initializer lists to call constructors with non-trivial logic or if you care that you are calling some particular constructor. Those should look like function calls using parentheses rather than like aggregate initialization.

Similarly, if you need to explicitly name a type and call its constructor to create a temporary, do not use a braced initializer list. Instead, use a braced initializer list without any type only when doing aggregate initialization or something notionally equivalent.

Examples:

```cpp
class Buffer {
public:
  // Construct a Buffer by reading data from a file.
  Buffer(std::string Filename);

  // Construct a Buffer by looking up the Nth prebuilt entry.
  Buffer(int N);
};

// The Buffer constructor call is reading a file, so do not use braces.
consumeBuffer(Buffer("input.dat"));

// The pair is being constructed like an aggregate, so use braces.
Map.insert({Key, Value});
```

If you use a braced initializer list when initializing a variable, use an equals sign before the open curly brace:

```cpp
int Data[] = {0, 1, 2, 3};
```

#### Use `auto` Type Deduction to Make Code More Readable

Use `auto` if and only if it makes the code more readable or easier to maintain. Do not almost always use `auto`, but do use it with initializers like `cast<Foo>(...)` or in other places where the type is already obvious from the context.

```cpp
auto *Node = cast<BinaryNode>(Expr);
auto It = Values.find(Key);
```

Avoid `auto` when it hides an important type or makes ownership and semantics harder to understand.

#### Beware Unnecessary Copies with `auto`

The convenience of `auto` makes it easy to forget that its default behavior is a copy. Particularly in range-based `for` loops, careless copies are expensive.

Use `auto &` for values and `auto *` for pointers unless you need to make a copy.

```cpp
// Typically there is no reason to copy.
for (const auto &Value : Values)
  observe(Value);

for (auto &Value : Values)
  Value.mutate();

// Remove the reference if you really want a new copy.
for (auto Value : Values) {
  Value.mutate();
  saveSomewhere(Value);
}

// Copy pointers, but make it clear that they are pointers.
for (const auto *Ptr : Pointers)
  observe(*Ptr);

for (auto *Ptr : Pointers)
  Ptr->mutate();
```

#### Beware of Non-Determinism Due to Ordering of Pointers

In general, there is no relative ordering among pointers. As a result, when unordered containers like sets and maps are used with pointer keys, the iteration order is undefined. Iterating such containers may therefore result in non-deterministic behavior.

If an ordered result is expected, remember to sort an unordered container before iteration or use ordered containers such as `vector`, `MapVector`, or `SetVector` if you want to iterate pointer keys.

#### Beware of Non-Deterministic Sorting Order of Equal Elements

`std::sort` uses a non-stable sorting algorithm in which the order of equal elements is not guaranteed to be preserved. Thus, using `std::sort` for a container having equal elements may result in non-deterministic behavior.

Default to using `llvm::sort` instead of `std::sort`.

## Style Issues

### The High-Level Issues

#### Self-Contained Headers

Header files should be self-contained and should compile on their own. In Mage, public headers should generally end in `.hpp`. Non-header files that are meant only for textual inclusion should end in `.inc` and should be used sparingly.

All header files should be self-contained. Users and refactoring tools should not have to adhere to special conditions to include a header. Specifically, a header should have header guards and include all other headers it needs.

There are rare cases where a file designed to be included is not self-contained. These are typically intended to be included at unusual locations, such as the middle of another file. They might not use header guards and might not include their prerequisites. Name such files with the `.inc` extension. Use them sparingly, and prefer self-contained headers when possible.

In general, a header should be implemented by one or more `.cpp` files. Each of these `.cpp` files should include the header that defines their interface first. This ensures that all of the dependencies of the header have been properly added to the header itself and are not implicit.

#### Library Layering

A directory of public headers, such as `include/mage/Support`, defines a library layer. One library, including both its headers and implementation, should only use things from the libraries listed in its dependencies.

Some of this constraint can be enforced by classic Unix linkers. A Unix linker searches left to right through the libraries specified on its command line and never revisits a library. In this way, no circular dependencies between libraries can exist.

This does not fully enforce all inter-library dependencies, and importantly it does not enforce header-file circular dependencies created by inline functions. A good way to answer the question `is this layered correctly?` is to consider whether a traditional Unix linker would succeed at linking the program if all inline functions were defined out of line.

Avoid circular dependencies between libraries, both at link time and through inline or header-only usage.

#### `#include` as Little as Possible

`#include` hurts compile-time performance. Do not include a header unless you have to, especially in header files.

If you are using a pointer or reference to a class, you often do not need the full definition of that class. If you are simply returning a class instance from a function declaration, you often do not need it either. In many cases, a forward declaration is enough.

Still, you must include all of the headers your file actually relies on. Do not rely on accidental transitive includes.

#### Keep Internal Headers Private

Many modules have a complex implementation that causes them to use more than one implementation file. It is often tempting to put the internal communication interface, helper classes, or extra functions in the public module header file. Do not do this.

If you really need to do something like this, put a private header file in the same directory as the source files and include it locally. This ensures that the private interface remains private and undisturbed by outsiders.

It is okay to put extra implementation methods in a public class itself. Just make them private or protected.

#### Use Namespace Qualifiers to Define Previously Declared Symbols

When providing an out-of-line definition for variables, functions, or opaque classes in a source file, do not open namespace blocks in the source file. Instead, use namespace qualifiers to help ensure that your definition matches an existing declaration.

Do this:

```cpp
// Foo.hpp
namespace mage {
extern int FooValue;
int foo(const char *S);

namespace detail {
class FooImpl;
} // namespace detail
} // namespace mage

// Foo.cpp
#include "Foo.hpp"
using namespace mage;

int mage::FooValue;

int mage::foo(const char *S) {
  // ...
}

class detail::FooImpl {
  // ...
};
```

Avoid this:

```cpp
// Foo.cpp
#include "Foo.hpp"

namespace mage {
int foo(char *S) { // Mismatch between "const char *" and "char *"
}
} // namespace mage
```

This error will not be caught until the build is nearly complete, when the linker fails to find a definition for uses of the original function.

Class method implementations must already name the class, and new overloads cannot be introduced out of line, so this recommendation does not apply to them.

#### Use Early Exits and `continue` to Simplify Code

When reading code, keep in mind how much state and how many previous decisions have to be remembered by the reader to understand a block of code. Aim to reduce indentation where possible when it does not make the code harder to understand.

Avoid:

```cpp
Value *doSomething(Instruction *I) {
  if (!I->isTerminator() && I->hasOneUse() && doOtherThing(I)) {
    // ... long code ...
  }

  return nullptr;
}
```

Preferred:

```cpp
Value *doSomething(Instruction *I) {
  if (I->isTerminator())
    return nullptr;

  if (!I->hasOneUse())
    return nullptr;

  if (!doOtherThing(I))
    return nullptr;

  // ... long code ...
}
```

A similar pattern applies to loops. If a loop becomes nested and hard to read, use `continue` early:

```cpp
for (Instruction &I : BB) {
  auto *BO = dyn_cast<BinaryOperator>(&I);
  if (!BO)
    continue;

  Value *LHS = BO->getOperand(0);
  Value *RHS = BO->getOperand(1);
  if (LHS == RHS)
    continue;

  // ...
}
```

#### Do Not Use `else` After `return`

For similar reasons as above, do not use `else` or `else if` after something that interrupts control flow such as `return`, `break`, `continue`, or `goto`.

Avoid:

```cpp
case 'J': {
  if (Signed) {
    Type = Context.getsigjmp_bufType();
    if (Type.isNull()) {
      Error = Context::MissingSigjmpBuf;
      return QualType();
    } else {
      break; // Unnecessary.
    }
  } else {
    Type = Context.getjmp_bufType();
    if (Type.isNull()) {
      Error = Context::MissingJmpBuf;
      return QualType();
    } else {
      break; // Unnecessary.
    }
  }
}
```

It is better to write it like this:

```cpp
case 'J':
  if (Signed) {
    Type = Context.getsigjmp_bufType();
    if (Type.isNull()) {
      Error = Context::MissingSigjmpBuf;
      return QualType();
    }
  } else {
    Type = Context.getjmp_bufType();
    if (Type.isNull()) {
      Error = Context::MissingJmpBuf;
      return QualType();
    }
  }
  break;
```

Or better yet, in this case:

```cpp
case 'J':
  if (Signed)
    Type = Context.getsigjmp_bufType();
  else
    Type = Context.getjmp_bufType();

  if (Type.isNull()) {
    Error = Signed ? Context::MissingSigjmpBuf : Context::MissingJmpBuf;
    return QualType();
  }
  break;
```

The idea is to reduce indentation and the amount of code you have to keep track of when reading the code.

Note: this advice does not apply to `if constexpr`. The substatement of the `else` clause may be a discarded statement, so removing the `else` can cause unexpected template instantiations. Thus, the following example is correct:

```cpp
template <typename T>
static constexpr bool VarTempl = true;

template <typename T>
int func() {
  if constexpr (VarTempl<T>)
    return 1;
  else
    static_assert(!VarTempl<T>);
}
```

#### Turn Predicate Loops into Predicate Functions

It is common to write small loops that just compute a boolean value. Prefer turning these into predicate functions with descriptive names.

Avoid:

```cpp
bool FoundFoo = false;
for (unsigned I = 0, E = BarList.size(); I != E; ++I)
  if (BarList[I]->isFoo()) {
    FoundFoo = true;
    break;
  }

if (FoundFoo) {
  // ...
}
```

Preferred:

```cpp
/// \returns true if the specified list has an element that is a foo.
static bool containsFoo(const std::vector<Bar *> &List) {
  for (unsigned I = 0, E = List.size(); I != E; ++I)
    if (List[I]->isFoo())
      return true;
  return false;
}

if (containsFoo(BarList)) {
  // ...
}
```

### The Low-Level Issues

#### Name Types, Functions, Variables, and Enumerators Properly

Poorly chosen names can mislead the reader and cause bugs. Use descriptive names. Pick names that match the semantics and role of the underlying entities, within reason. Avoid abbreviations unless they are well known. After picking a good name, make sure to use consistent capitalization for the name, as inconsistency requires clients to either memorize the APIs or look them up to find the exact spelling.

In general, names should be in camel case. Different kinds of declarations have different rules:

- **Type names** should be nouns and start with an upper-case letter, for example `TextFileReader`.
- **Variable names** should be nouns. The name should be camel case and start with an upper-case letter, for example `Leader` or `Boats`.
- **Function names** should be verb phrases, and command-like functions should be imperative. The name should be camel case and start with a lowercase letter, for example `openFile()` or `isFoo()`.
- **Enum declarations** are types, so they should follow the naming conventions for types. A common use for enums is as a discriminator for a union or an indicator of a subclass. When an enum is used for something like this, it should often have a `Kind` suffix, for example `ValueKind`.
- **Enumerators** and **public member variables** should start with an upper-case letter. Unless the enumerators are defined in their own small namespace or inside a class, enumerators should have a prefix corresponding to the enum declaration name. For example, `enum ValueKind { ... };` may contain enumerators like `VK_Argument`, `VK_BasicBlock`, and so on. Enumerators that are just convenience constants are exempt from the requirement for a prefix.

For example:

```cpp
enum {
  MaxSize = 42,
  Density = 12
};
```

As an exception, classes that intentionally mimic STL classes can have member names in the STL style of lowercase words separated by underscores, such as `begin()`, `push_back()`, and `empty()`. Classes that provide multiple iterators should add a singular prefix to `begin()` and `end()`, such as `global_begin()` and `use_begin()`.

Here are some examples:

```cpp
class VehicleMaker {
  Factory<Tire> F;            // Avoid: a non-descriptive abbreviation.
  Factory<Tire> Factory;      // Better: more descriptive.
  Factory<Tire> TireFactory;  // Even better: if VehicleMaker has more than one kind of factory.
};

Vehicle makeVehicle(VehicleType Type) {
  VehicleMaker Maker;                     // Might be OK if scope is small.
  Tire Tmp1 = Maker.makeTire();           // Avoid: `Tmp1` provides no information.
  Light Headlight = Maker.makeLight("head");  // Good: descriptive.
  // ...
}
```

#### Assert Liberally

Use the `assert` macro to its fullest. Check all of your preconditions and assumptions.

Make sure to put some kind of error message in the assertion statement so that the message is printed if the assertion is tripped:

```cpp
inline Value *getOperand(unsigned I) {
  assert(I < Operands.size() && "getOperand() out of range");
  return Operands[I];
}
```

Here are more examples:

```cpp
assert(Ty->isPointerType() && "cannot allocate a non-pointer type");
assert((Opcode == Shl || Opcode == Shr) && "ShiftInst opcode invalid");
assert(Idx < getNumSuccessors() && "successor index out of range");
assert(V1.getType() == V2.getType() && "constant types must be identical");
```

Do not use `assert(false)` for unreachable code. Use `llvm_unreachable(...)` instead:

```cpp
llvm_unreachable("invalid radix for integer literal");
```

If the error condition can be triggered by user input, use an appropriate recoverable error mechanism instead of an assertion. In cases where this is not practical, `report_fatal_error` may be used.

Another issue is that values used only by assertions will produce an unused-variable warning when assertions are disabled. In the first case below, the call should be moved into the assertion. In the second case, the side effects must happen whether the assert is enabled or not, so `[[maybe_unused]]` is appropriate:

```cpp
assert(V.size() > 42 && "vector smaller than it should be");

[[maybe_unused]] bool NewToSet = MySet.insert(Value);
assert(NewToSet && "the value should not already be in the set");
```

#### Do Not Use `using namespace std` or `using namespace llvm`

Do not use `using namespace std;` or `using namespace llvm;`.

In header files, do not use `using namespace ...;`. A namespace directive in a header pollutes the namespace of every translation unit that includes the header, creating maintenance issues.

In implementation files, explicitly qualifying namespaces makes the code clearer, because it is immediately obvious what facilities are being used and where they are coming from. It also makes the code more portable, because namespace clashes cannot occur between Mage code, LLVM, the C++ standard library, and other namespaces.

In `.cpp` files that clearly implement code in the `mage` namespace, `using namespace mage;` may be acceptable at the top of the file, after the `#include`s, when doing so materially improves readability. This is the only general exception here: an implementation file may use the namespace it is implementing, but should not use other namespaces this way.

#### Provide a Virtual Method Anchor for Classes in Headers

If a class is defined in a header file and has a vtable, either because it has virtual methods or derives from classes with virtual methods, it must always have at least one out-of-line virtual method in the class. Without this, the compiler may copy the vtable and related metadata into every object file that includes the header.

#### Do Not Use Default Labels in Fully Covered Switches Over Enumerations

If you write a `default` label on a fully covered switch over an enumeration, the compiler warning for missing enumeration cases will not fire when new elements are added to that enumeration.

Avoid:

```cpp
switch (Kind) {
case VK_Argument:
  return handleArgument();
case VK_BasicBlock:
  return handleBasicBlock();
default:
  llvm_unreachable("unexpected value kind");
}
```

Preferred:

```cpp
switch (Kind) {
case VK_Argument:
  return handleArgument();
case VK_BasicBlock:
  return handleBasicBlock();
}

llvm_unreachable("unexpected value kind");
```

#### Use Range-Based `for` Loops Wherever Possible

Use range-based `for` loops wherever possible for all newly added code:

```cpp
for (Instruction &I : *BB)
  observe(I);
```

Usage of `std::for_each()` or `llvm::for_each()` is discouraged unless the callable object already exists.

#### Do Not Evaluate `end()` Every Time Through a Loop

In cases where range-based `for` loops cannot be used and it is necessary to write an explicit iterator-based loop, pay close attention to whether `end()` is reevaluated on each loop iteration.

Avoid:

```cpp
for (auto I = BB->begin(); I != BB->end(); ++I)
  use(*I);
```

Preferred:

```cpp
for (auto I = BB->begin(), E = BB->end(); I != E; ++I)
  use(*I);
```

If you intentionally depend on the changing value of `end()` because the container is being mutated, write the loop in the first form and add a comment indicating that you did it intentionally.

#### `#include <iostream>` Is Forbidden

The use of `#include <iostream>` in library files is forbidden because many common implementations transparently inject a static constructor into every translation unit that includes it.

Using the other stream headers, such as `<sstream>`, is not problematic in this regard. However, `raw_ostream` provides APIs that are better performing for almost every use than `std::ostream` style APIs.

#### Use `raw_ostream`

LLVM includes a lightweight, simple, and efficient stream implementation in `llvm/Support/raw_ostream.h`, which provides common features of `std::ostream`. All new code should use `raw_ostream` instead of `ostream`.

Unlike `std::ostream`, `raw_ostream` is not a template and can be forward declared as `class raw_ostream`. Public headers should generally not include the `raw_ostream` header, but use forward declarations and constant references to `raw_ostream` instances.

```cpp
namespace llvm {
class raw_ostream;
}

class Widget {
public:
  void print(llvm::raw_ostream &OS) const;
};
```

#### Avoid `std::endl`

`std::endl` outputs a newline and also flushes the output stream. Most of the time there is no reason to flush the output stream, so it is better to use a literal `'\n'`.

Avoid:

```cpp
OS << "done" << std::endl;
```

Preferred:

```cpp
OS << "done\n";
```

#### Do Not Use `inline` When Defining a Function in a Class Definition

A member function defined in a class definition is implicitly inline, so do not put the `inline` keyword in this case.

Avoid:

```cpp
class Foo {
public:
  inline void bar() {
    // ...
  }
};
```

Preferred:

```cpp
class Foo {
public:
  void bar() {
    // ...
  }
};
```

### Microscopic Details

#### Spaces Before Parentheses

Put a space before an open parenthesis only in control-flow statements, but not in normal function call expressions and function-like macros.

```cpp
if (X)
  foo();

for (I = 0; I != 100; ++I)
  bar();

while (Ready)
  step();

somefunc(42);
assert(3 != 4 && "laws of math are failing me");

A = foo(42, 92) + bar(X);
```

#### Prefer Preincrement

Preincrement (`++X`) may be no slower than postincrement (`X++`) and could be a lot faster. Use preincrement whenever possible.

The semantics of postincrement include making a copy of the value being incremented, returning it, and then preincrementing the work value. For primitive types, this is not usually a big deal. For iterators, it can be expensive.

Preferred:

```cpp
for (auto I = Values.begin(), E = Values.end(); I != E; ++I)
  use(*I);
```

#### Namespace Indentation

Do not indent namespaces.

```cpp
namespace mage {
namespace support {

class Grokable {
public:
  explicit Grokable() = default;
};

} // namespace support
} // namespace mage
```

Feel free to skip the closing comment when the namespace being closed is obvious for any reason. For example, the outermost namespace in a header file is rarely a source of confusion.

#### Restrict Visibility

Functions and variables should have the most restricted visibility possible.

For class members, that means using appropriate `private`, `protected`, or `public` access.

For non-member functions, variables, and classes, that means restricting visibility to a single `.cpp` file if they are not referenced outside that file.

Visibility of file-scope non-member variables and functions can be restricted to the current translation unit by using either the `static` keyword or an anonymous namespace.

Anonymous namespaces are more general than `static` because they can make entire classes private to a file, but they also reduce locality of reference. Therefore, make anonymous namespaces as small as possible, and only use them for class declarations.

Preferred:

```cpp
namespace {
class StringSort {
public:
  bool operator<(const char *RHS) const;
};
} // namespace

static void runHelper() {
  // ...
}
```

Avoid putting declarations other than classes into anonymous namespaces:

```cpp
namespace {

// ... many declarations ...

void runHelper() {
  // ...
}

// ... many declarations ...

} // namespace
```

For file-local functions, `static` is often clearer.

#### Do Not Use Braces on Simple Single-Statement Bodies of `if`/`else`/Loop Statements

When writing the body of an `if`, `else`, or `for`/`while` loop statement, aim to reduce unnecessary line noise.

**Omit braces when:**

- the body consists of a single **simple** statement;
- the single statement is not preceded by a comment;
  (hoist comments above the control statement if you can);
- an `else` clause, if present, also meets the above criteria (single simple statement, no associated comments).

**Use braces in all other cases, including:**

- multi-statement bodies;
- single-statement bodies with non-hoistable comments;
- complex single-statement bodies (for example, deep nesting or complex nested loops);
- inconsistent bracing within `if` / `else if` / `else` chains (if one block requires braces, all must);
- `if` statements ending with a nested `if` lacking an `else` (to prevent a dangling `else`).

The examples below provide guidelines for these cases:

```cpp
// Omit the braces since the body is simple and clearly associated with the
// `if`.
if (isa<FunctionDecl>(D))
  handleFunctionDecl(D);
else if (isa<VarDecl>(D))
  handleVarDecl(D);

// Here we document the condition itself and not the body.
if (isa<VarDecl>(D)) {
  // It is necessary that we explain the situation with this surprisingly long
  // comment, so it would be unclear without the braces whether the following
  // statement is in the scope of the `if`.
  // Because the condition is documented, we can't really hoist this
  // comment that applies to the body above the `if`.
  handleOtherDecl(D);
}

// Use braces on the outer `if` to avoid a potential dangling `else`
// situation.
if (isa<VarDecl>(D)) {
  if (shouldProcessAttr(A))
    handleAttr(A);
}

// Use braces for the `if` block to keep it uniform with the `else` block.
if (isa<FunctionDecl>(D)) {
  handleFunctionDecl(D);
} else {
  // In this `else` case, it is necessary that we explain the situation with
  // this surprisingly long comment, so it would be unclear without the braces
  // whether the following statement is in the scope of the `if`.
  handleOtherDecl(D);
}

// Use braces for the `else if` and `else` block to keep it uniform with the
// `if` block.
if (isa<FunctionDecl>(D)) {
  verifyFunctionDecl(D);
  handleFunctionDecl(D);
} else if (isa<GlobalVarDecl>(D)) {
  handleGlobalVarDecl(D);
} else {
  handleOtherDecl(D);
}

// This should also omit braces. The `for` loop contains only a single
// statement, so it should not have braces. The `if` also only contains a
// single simple statement (the `for` loop), so it also should omit braces.
if (isa<FunctionDecl>(D))
  for (auto *A : D.attrs())
    handleAttr(A);

// Use braces for a `do-while` loop and its enclosing statement.
if (Tok->is(tok::l_brace)) {
  do {
    Tok = Tok->Next;
  } while (Tok);
}

// Use braces for the outer `if` since the nested `for` is braced.
if (isa<FunctionDecl>(D)) {
  for (auto *A : D.attrs()) {
    // In this `for` loop body, it is necessary that we explain the situation
    // with this surprisingly long comment, forcing braces on the `for` block.
    handleAttr(A);
  }
}

// Use braces on the outer block because there are more than two levels of
// nesting.
if (isa<FunctionDecl>(D)) {
  for (auto *A : D.attrs())
    for (ssize_t I : llvm::seq<ssize_t>(Count))
      handleAttrOnDecl(D, A, I);
}

// Use braces on the outer block because of a nested `if`; otherwise, the
// compiler would warn: `add explicit braces to avoid dangling else`
if (auto *FD = dyn_cast<FunctionDecl>(D)) {
  if (shouldProcess(FD))
    handleVarDecl(FD);
  else
    markAsIgnored(FD);
}
```
