# VfsShell Module Refactoring Guide

## Overview

This document provides a comprehensive guide for splitting the monolithic `VfsShell/codex.cpp` (4118 lines) and `VfsShell/codex.h` into clean, logical modules.

## Current Structure

### codex.h (384 lines)
- Lines 1-58: Headers and tracing macros
- Lines 60-93: VFS base types (VfsNode, DirNode, FileNode)
- Lines 94-170: Value and S-expression AST types
- Lines 172-234: Vfs class declaration
- Lines 235: extern G_VFS
- Lines 238-243: Parser declarations
- Lines 245-248: Builtins
- Lines 250-254: Exec utils
- Lines 256-373: C++ AST node types
- Lines 375-382: AI bridge functions

### codex.cpp (4118 lines)
- Lines 1-66: Trace implementation, includes
- Lines 68-105: Utility functions (trim, path ops)
- Lines 106-598: Overlay serialization/deserialization
- Lines 598-690: Binary reader/writer helpers
- Lines 690-818: S-expression AST serialization
- Lines 818-1283: C++ AST serialization
- Lines 1283-1578: Terminal handling (RawTerminalMode)
- Lines 1578-1863: Command pipeline and execution
- Lines 1863-1927: Value::show and AST node implementations
- Lines 1928-2307: VFS implementation
- Lines 2308-2391: S-expression parser
- Lines 2392-2534: Builtins implementation
- Lines 2535-2586: Exec utilities
- Lines 2588-2802: C++ AST nodes implementation
- Lines 2803-3202: AI bridge (OpenAI and Llama)
- Lines 3203-end: REPL and main()

## Target Module Structure

### 1. utils.h / utils.cpp
**Purpose**: String utilities, path utilities, exec functions

**Header contents**:
- `std::string trim_copy(const std::string& s)`
- `std::string join_args(const std::vector<std::string>& args, size_t start = 0)`
- `std::string join_path(const std::string& base, const std::string& leaf)`
- `std::string normalize_path(const std::string& cwd, const std::string& operand)`
- `std::string path_basename(const std::string& path)`
- `std::string exec_capture(const std::string& cmd, const std::string& desc = {})`
- `bool has_cmd(const std::string& c)`

**Source extracts**:
- Lines 68-105 (path/string utils)
- Lines 2535-2586 (exec_capture, has_cmd)
- Line 1231-1239 (join_args helper)

**Status**: ✓ COMPLETED

### 2. vfs.h / vfs.cpp
**Purpose**: VfsNode, DirNode, FileNode, Vfs class

**Header contents**:
- Extract lines 60-93 from codex.h (VfsNode hierarchy)
- Extract lines 172-234 from codex.h (Vfs class)
- `extern Vfs* G_VFS;`

**Source extracts**:
- Lines 1928-2307 (VFS implementation)
- Helper functions: traverse_optional, type_char

**Dependencies**:
- Needs: utils.h (for join_path)
- Forward declare: AstNode

### 3. value.h / value.cpp
**Purpose**: Value struct, Env struct, S-expression AST nodes

**Header contents**:
- Extract lines 94-170 from codex.h
- Value struct with Closure, Builtin, List
- Env struct
- AstNode base and all derived types (AstInt, AstBool, AstStr, AstSym, AstIf, AstLambda, AstCall, AstHolder)

**Source extracts**:
- Lines 1863-1872 (Value::show)
- Lines 1874-1887 (AST node constructors)
- Lines 1888-1927 (AST eval methods)

**Dependencies**:
- Needs: vfs.h (AstNode inherits from VfsNode)

### 4. parser.h / parser.cpp
**Purpose**: S-expression lexer and parser

**Header contents**:
- Extract lines 238-243 from codex.h
- `struct Token { std::string s; };`
- `std::vector<Token> lex(const std::string& src);`
- `std::shared_ptr<AstNode> parse(const std::string& src);`

**Source extracts**:
- Lines 2308-2391 (complete parser implementation)
- Static helpers: POS, parseExpr, isInt, atom, parseList

**Dependencies**:
- Needs: value.h (for AstNode types)

### 5. builtins.h / builtins.cpp
**Purpose**: Builtin functions for S-expression language

**Header contents**:
- Extract lines 245-248 from codex.h
- `void install_builtins(std::shared_ptr<Env> g);`

**Source extracts**:
- Lines 2392-2534 (complete builtins)
- Implements: +, -, *, =, <, print, list, cons, head, tail, null?, str.cat, str.sub, str.find, vfs-write, vfs-read, vfs-ls, export, sys, cpp:hello

**Dependencies**:
- Needs: value.h, vfs.h
- Uses: G_VFS, exec_capture

### 6. cpp_ast.h / cpp_ast.cpp
**Purpose**: C++ AST node types

**Header contents**:
- Extract lines 256-373 from codex.h
- CppNode base class
- All C++ AST types: CppInclude, CppExpr, CppId, CppString, CppInt, CppCall, CppBinOp, CppStreamOut, CppRawExpr, CppStmt, CppExprStmt, CppReturn, CppRawStmt, CppVarDecl, CppCompound, CppParam, CppFunction, CppRangeFor, CppTranslationUnit
- Helper functions: expect_tu, expect_fn, expect_block, vfs_add, cpp_dump_to_vfs

**Source extracts**:
- Lines 2588-2802 (C++ AST implementations)
- Lines 2596-2630 (verify_cpp_string_literal helper)

**Dependencies**:
- Needs: value.h (CppNode inherits from AstNode)
- Needs: vfs.h

### 7. ai_bridge.h / ai_bridge.cpp
**Purpose**: AI functions for OpenAI and Llama

**Header contents**:
- Extract lines 375-382 from codex.h
- `std::string json_escape(const std::string& s);`
- `std::string build_responses_payload(const std::string& model, const std::string& user_prompt);`
- `std::string call_openai(const std::string& prompt);`
- `std::string call_llama(const std::string& prompt);`
- `std::string call_ai(const std::string& prompt);`

**Source extracts**:
- Lines 2803-3202 (complete AI bridge)
- Includes: system_prompt_text, build_chat_payload, hex_value, append_utf8, decode_unicode_escape_sequence, decode_json_string, json_string_value_after_colon
- AI caching functions

**Dependencies**:
- Needs: utils.h (exec_capture, has_cmd)

### 8. overlay.h / overlay.cpp
**Purpose**: Overlay serialization/deserialization and working directory context

**Header contents**:
- WorkingDirectory struct
- SolutionContext struct
- Serialization functions:
  - `std::pair<std::string, std::string> serialize_ast_node(const std::shared_ptr<AstNode>& node);`
  - `std::shared_ptr<AstNode> deserialize_ast_node(...);`
  - `size_t mount_overlay_from_file(Vfs& vfs, const std::string& name, const std::string& hostPath);`
  - `void save_overlay_to_file(Vfs& vfs, size_t overlayId, const std::string& hostPath);`
- Solution management functions

**Source extracts**:
- Lines 106-598 (overlay mount/save/serialization)
- Lines 598-690 (BinaryWriter/BinaryReader)
- Lines 690-818 (S-expression AST serialization)
- Lines 818-1283 (C++ AST serialization)
- WorkingDirectory/SolutionContext helpers (lines 113-246)

**Dependencies**:
- Needs: vfs.h, value.h, cpp_ast.h, utils.h

### 9. terminal.h / terminal.cpp
**Purpose**: Terminal handling and raw mode

**Header contents**:
- `bool terminal_available();`
- RawTerminalMode class
- History functions
- Line editing functions

**Source extracts**:
- Lines 1240-1282 (history file handling)
- Lines 1283-1577 (RawTerminalMode and line editing)

**Dependencies**:
- Standard library only

### 10. shell.h / shell.cpp
**Purpose**: Command pipeline, REPL, main()

**Header contents**:
- CommandInvocation, CommandPipeline, CommandChainEntry, CommandResult structs
- Command execution functions
- REPL functions

**Source extracts**:
- Lines 1578-1863 (command pipeline)
- Lines 1717-1795 (line parsing helpers)
- Lines 1796-1861 (cache and RNG helpers)
- Lines 3203-end (REPL and main())

**Dependencies**:
- Needs: ALL other modules

### 11. codex.h (updated)
**Purpose**: Main header that includes all modules

**New structure**:
```cpp
#ifndef _VfsShell_codex_h_
#define _VfsShell_codex_h_

// Standard library includes
#include <algorithm>
#include <array>
// ... (keep existing standard headers)

// Tracing macros (keep as-is, lines 23-58)
#ifdef CODEX_TRACE
// ... tracing implementation ...
#endif

// Module headers
#include "utils.h"
#include "vfs.h"
#include "value.h"
#include "parser.h"
#include "builtins.h"
#include "cpp_ast.h"
#include "ai_bridge.h"
#include "overlay.h"
#include "terminal.h"
#include "shell.h"

#endif
```

### 12. codex.cpp (updated)
**Purpose**: Minimal file or removed entirely

**Options**:
1. Remove entirely (all code moved to modules)
2. Keep only for main() if not moved to shell.cpp

## Makefile Updates

Update `VFSSHELL_SRC` to include all new source files:

```makefile
VFSSHELL_SRC := VfsShell/codex.cpp \
                VfsShell/utils.cpp \
                VfsShell/vfs.cpp \
                VfsShell/value.cpp \
                VfsShell/parser.cpp \
                VfsShell/builtins.cpp \
                VfsShell/cpp_ast.cpp \
                VfsShell/ai_bridge.cpp \
                VfsShell/overlay.cpp \
                VfsShell/terminal.cpp \
                VfsShell/shell.cpp \
                VfsShell/snippet_catalog.cpp

VFSSHELL_HDR := VfsShell/codex.h \
                VfsShell/utils.h \
                VfsShell/vfs.h \
                VfsShell/value.h \
                VfsShell/parser.h \
                VfsShell/builtins.h \
                VfsShell/cpp_ast.h \
                VfsShell/ai_bridge.h \
                VfsShell/overlay.h \
                VfsShell/terminal.h \
                VfsShell/shell.h \
                VfsShell/snippet_catalog.h
```

## Implementation Strategy

### Phase 1: Prepare
1. Backup current files: `codex.cpp.bak`, `codex.h.bak`
2. Ensure current code compiles: `make clean && make`
3. Run tests to establish baseline: `make sample`

### Phase 2: Create Headers
1. Create all .h files with proper declarations
2. Add include guards
3. Add forward declarations where needed
4. Keep tracing macros accessible

### Phase 3: Create Source Files
1. Extract implementations to .cpp files
2. Add necessary #includes
3. Maintain static helper functions in anonymous namespaces

### Phase 4: Update Main Files
1. Update codex.h to include all module headers
2. Minimize or remove codex.cpp
3. Update Makefile

### Phase 5: Test
1. Compile: `make clean && make`
2. Fix compilation errors iteratively
3. Run sample: `make sample`
4. Run full test suite: `python tools/test_harness.py`

### Phase 6: Verify
1. Test all major features
2. Check for memory leaks if applicable
3. Verify no behavioral changes
4. Commit to git

## Key Challenges

1. **Circular Dependencies**: value.h needs vfs.h (AstNode extends VfsNode), but vfs.h may need value.h. Solution: Use forward declarations.

2. **Static Helper Functions**: Many static functions in codex.cpp. Solution: Keep them in anonymous namespaces in the appropriate .cpp file.

3. **G_VFS Global**: Declared extern in vfs.h, defined in vfs.cpp.

4. **Tracing Macros**: Keep in codex.h so all modules can use them.

5. **normalize_path**: Depends on Vfs::splitPath. Solution: Implement in vfs.cpp or utils.cpp with proper includes.

## Testing Checklist

After split, verify:
- [ ] `make clean && make` succeeds
- [ ] `make sample` succeeds
- [ ] All test scripts in `tests/` run correctly
- [ ] `python tools/test_harness.py` passes
- [ ] REPL starts and accepts commands
- [ ] VFS operations work (ls, tree, mkdir, touch, cat)
- [ ] S-expression evaluation works
- [ ] C++ AST generation works
- [ ] AI bridge responds (if keys configured)
- [ ] Overlay save/load works
- [ ] Solution workflow functions

## Notes

- This is a large refactoring touching 4100+ lines
- Expect multiple compilation iterations
- Test frequently during the process
- Keep git commits incremental
- Document any behavioral changes
- Update TASKS.md when complete

## Status

- [x] utils.h/cpp created
- [ ] vfs.h/cpp
- [ ] value.h/cpp
- [ ] parser.h/cpp
- [ ] builtins.h/cpp
- [ ] cpp_ast.h/cpp
- [ ] ai_bridge.h/cpp
- [ ] overlay.h/cpp
- [ ] terminal.h/cpp
- [ ] shell.h/cpp
- [ ] codex.h updated
- [ ] Makefile updated
- [ ] Compilation test
- [ ] Functionality test
