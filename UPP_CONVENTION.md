## Ultimate++ Package Conventions

The **Ultimate++ (U++)** framework organizes all code into *assemblies* and *packages*.

Each **package** is a directory containing a `.upp` file with the same name as the directory itself:
```

abc/abc.upp

```

Every package also has a **main header**, named after the package:
```

abc/abc.h

```
This main header is the **only** header that should include other headers.
Other `.h` files within the same package must not contain `#include` directives except for **inline includes** (see below).

### Inline Includes
If template classes or inline functions require definitions within headers, these may be placed in `.hpp` files.
Such `.hpp` files are included where necessary using inline includes:
```

#include "template_impl.hpp" // allowed only for inline definitions

```

### Headers
Don't use ```#pragma once``` but the style of

```
#ifndef _daw_daw_h_
#define _daw_daw_h_

// all content of the file

#endif
```

Don't have dependency ```#include``` statements in non-main headers.

The compiler already sees the classes, but YOU don't. For example, don't add "#include <AudioCore/AudioCore.h>" to "src/AudioFX/Limiter.h" for the compiler, add it as a COMMENT just for YOU.

Fix the problems by looking for "\n#include" in ".h" files and if it's not a main header, convert it to a comment. Almost the same applies to c/cpp files: look for "\n#include" in ".cpp" files and if it's not a main header, convert it to a comment, UNLESS it's really only needed in the implementation, which is very possible, but check if it's already in the main header.

And I repeat, this doesn't change any architectural changes. This ONLY affects the preprocessor. The ONLY problem is that YOU don't understand how to include headers and code dependencies.

This means that you can't just add include statements and hope for the best, thinking that everything is fine now that the included headers are chained. You MUST also consider circular dependencies through the main headers. They would have broken silently even with the previous include technique.


### Forward declaration
Do all forward declarations in the beginning of main-header, so they don't need to be forward declared in every header.

AI may add comments, if it looks otherwise, that no classes have been included yet, since we don't use inside-non-main-header inclusions anymore.

### Inclusion Rules
- **Only the main header (`package/package.h`)** includes other headers.
- **All `.cpp` files** in a package must include *only the main header* of their package.
- Headers from *other packages* must be included by their main headers only (e.g. `#include <AudioCore/AudioCore.h>`).
- Other than main headers doesn't need to be self-contained, since implementation files only use packet's main header.

### Package Dependencies (`uses`)
If a package uses another package’s main header, it must explicitly declare this dependency in its `.upp` file via the `uses(...)` section.

For example:
```

uses
Core
Draw
CtrlCore
CtrlLib
AudioCore
AudioGraph

```

This ensures Ultimate++ (TheIDE) correctly manages compilation order, linking, and visibility across packages.

### Enforcement
When writing or maintaining code:
- Move all `#include` statements from non-main headers into the corresponding main header.
- Ensure `.cpp` files only include their package’s main header.
- Maintain clear `uses(...)` relationships between packages.

### Summary
U++ packages behave as independent modular units.
Their boundaries are defined by `.upp` manifests and main headers.
This convention guarantees consistency, avoids circular dependencies, and makes the codebase predictable for both Codex and human developers.

### Classes
There's no smart-pointers in U++ really, but you should use RAII.
This is not a proper solution:
```
Vector< Ptr<Node> > nodes;
```

Instead this is proper solution:
```
Array<Node> nodes;
```

For "smart-values" you may add raw classes to Value class, and then use proper value-based semantics. But no, there's no valid mid-ground between value-semantics and RAII.

Everything in Vector<> must be trivially copyable, but Array<> doesn't have that requirement. Pointers to Vector<> cannot be trusted almost ever. But you may trust pointers to vector ranges, if you take both begin and end pointers at the beginning. Even that should be used only in very fast loops. You may trust pointers to Array<>, but add additional Ptr/Pte semantics (struct Obj : Pte<Obj> {}; Ptr<Obj> o;) if you must be sure.

If class' methods uses THISBACK macros, then class definition must have e.g. (the class is Node) "typedef Node CLASSNAME;" statement.

So, because we use RAII, avoid using new operator everywhere. If it is being used, it must be contained in RAII containers immediately. It cannot be created just for stack memory, to a pointer in the middle of a function. You will NEVER use pointers for OWNED variables, but if you need to transfer ownership, you may create One<T> wrapper. You will NEVER need to delete values in destructor (unless you are making a container template class).



