# VfsShell Code Splitting Plan

## Current State
- **VfsShell/codex.cpp**: 4118 lines (too large, needs splitting)
- **VfsShell/codex.h**: 384 lines (contains all declarations)
- **VfsShell/utils.h/cpp**: Already created (partial implementation)
- **VfsShell/snippet_catalog.h/cpp**: Already separate

## Target Module Structure

### 1. utils.h/cpp
**Status**: ✅ Partially complete (needs normalize_path fix)
**Lines in codex.cpp**: 68-104, 2535-2586
**Contents**:
- `trim_copy()`, `json_escape()`
- `join_path()`, `normalize_path()`, `path_basename()`
- `exec_capture()`, `has_cmd()`

**Action**: Fix normalize_path implementation to avoid circular dependency

### 2. vfs.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 63-92 (VfsNode, DirNode, FileNode), 175-233 (Vfs class)
**Lines in codex.cpp**: 1928-2307
**Contents**:
- VfsNode base class and derived classes (DirNode, FileNode)
- Vfs class with overlay support
- All VFS operations (mkdir, touch, write, read, rm, mv, link, ls, tree)
- G_VFS global pointer

**Dependencies**: None (base module)

### 3. value.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 99-170 (Value, Env, AstNode types)
**Lines in codex.cpp**: 1863-1927
**Contents**:
- Value struct with variant types
- Env (environment) struct
- All S-expression AST node types (AstInt, AstBool, AstStr, AstSym, AstIf, AstLambda, AstCall, AstHolder)
- Value::show() implementation
- AST node constructors and eval() methods

**Dependencies**: vfs.h (AstNode inherits from VfsNode)

### 4. parser.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 240-242
**Lines in codex.cpp**: 2308-2391
**Contents**:
- Token struct
- `lex()` function
- `parse()` function
- Internal parser helpers (parseExpr, parseList, atom, isInt)

**Dependencies**: value.h (creates AST nodes)

### 5. builtins.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 247
**Lines in codex.cpp**: 2392-2534
**Contents**:
- `install_builtins()` function
- All builtin functions (+, -, *, =, <, print, list operations, string operations, VFS helpers, export, sys, cpp:hello)

**Dependencies**: value.h, vfs.h, utils.h

### 6. cpp_ast.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 258-372
**Lines in codex.cpp**: 2588-2802
**Contents**:
- CppNode base class
- All C++ AST node types (CppInclude, CppExpr types, CppStmt types, CppFunction, CppTranslationUnit, etc.)
- Helper functions (expect_tu, expect_fn, expect_block, vfs_add, cpp_dump_to_vfs)
- String escaping and validation for C++ code generation

**Dependencies**: value.h (inherits from AstNode), vfs.h

### 7. ai_bridge.h/cpp
**Status**: ❌ Not created
**Lines in codex.h**: 377-381
**Lines in codex.cpp**: 2803-3202
**Contents**:
- `json_escape()` (may move to utils)
- `build_responses_payload()`, `build_chat_payload()`
- JSON parsing helpers (decode_json_string, find_json_string_field, etc.)
- `call_openai()`, `call_llama()`, `call_ai()`
- OpenAI key loading
- AI response caching functions
- System prompt generation

**Dependencies**: utils.h, snippet_catalog.h

### 8. codex.h (updated)
**Status**: ❌ Needs updating
**Action**: Remove declarations that moved to module headers, add #includes for new modules

### 9. codex.cpp (reduced)
**Status**: ❌ Needs major reduction
**Keep**: Lines 1-67 (includes, trace implementation), 106-1862 (serialization, working directory, solution context, REPL helpers), 3203-4118 (main(), REPL, command dispatcher)
**Total**: ~2000 lines (down from 4118)

## Implementation Order

1. **Create vfs.h/cpp** (no dependencies on other new modules)
2. **Create value.h/cpp** (depends on vfs)
3. **Create parser.h/cpp** (depends on value)
4. **Create builtins.h/cpp** (depends on value, vfs, utils)
5. **Create cpp_ast.h/cpp** (depends on value, vfs)
6. **Create ai_bridge.h/cpp** (depends on utils, snippet_catalog)
7. **Update codex.h** (add includes, remove moved declarations)
8. **Update codex.cpp** (remove moved implementations, keep main/REPL/commands)
9. **Update Makefile** (add all new .cpp files to VFSSHELL_SRC)
10. **Test compilation**: `make clean && make`
11. **Fix compilation errors** iteratively
12. **Run tests**: `python tools/test_harness.py`

## Makefile Update

```makefile
VFSSHELL_SRC := VfsShell/codex.cpp \
                VfsShell/utils.cpp \
                VfsShell/vfs.cpp \
                VfsShell/value.cpp \
                VfsShell/parser.cpp \
                VfsShell/builtins.cpp \
                VfsShell/cpp_ast.cpp \
                VfsShell/ai_bridge.cpp \
                VfsShell/snippet_catalog.cpp

VFSSHELL_HDR := VfsShell/codex.h \
                VfsShell/utils.h \
                VfsShell/vfs.h \
                VfsShell/value.h \
                VfsShell/parser.h \
                VfsShell/builtins.h \
                VfsShell/cpp_ast.h \
                VfsShell/ai_bridge.h \
                VfsShell/snippet_catalog.h
```

## Key Challenges

1. **Circular Dependencies**:
   - normalize_path() needs Vfs::splitPath()
   - Solution: Make splitPath() a static method or move to utils

2. **Forward Declarations**:
   - Many types reference each other
   - Solution: Careful ordering of includes and forward declarations

3. **Global State**:
   - `G_VFS` pointer used throughout
   - Solution: Declare `extern` in vfs.h, define in vfs.cpp

4. **Template/Inline Functions**:
   - Some helpers may need to stay in headers
   - Solution: Mark as `inline` or keep in .h files

5. **Compilation Order**:
   - Must compile in dependency order
   - Solution: Makefile handles this automatically with proper includes

## Testing Strategy

1. After each module creation, compile with: `make clean && make`
2. Fix any compilation errors immediately
3. Once all modules compile, run: `make sample`
4. Finally run full test suite: `python tools/test_harness.py`

## Documentation

After code splitting is complete, create `VfsShell/AGENTS.md` explaining:
- Purpose of each module
- Dependencies between modules
- Key data structures and algorithms
- How to add new features to each module

## Expected Outcome

- codex.cpp: ~2000 lines (down from 4118)
- 7 new module pairs (14 files total)
- Cleaner, more maintainable codebase
- Easier to understand and modify
- Better compilation times (only rebuild changed modules)

## Next Steps

Human should execute this plan in order, testing at each step. The task is complex enough that automated splitting risks breaking the build, so manual, careful execution is recommended.
