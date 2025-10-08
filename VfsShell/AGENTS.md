# VfsShell Implementation Architecture

This document explains the file structure and code organization of the VfsShell codebase.

## Current File Structure

### Source Files

**codex.h** (384 lines) - Main header file
- Tracing macros (CODEX_TRACE support)
- Core VFS types: VfsNode, DirNode, FileNode
- Value type and S-expression AST nodes
- Environment (Env) for S-expression evaluation
- VFS class with overlay support
- S-expression parser declarations
- Builtin function interface
- C++ AST node types
- AI bridge function declarations

**codex.cpp** (4118 lines) - Main implementation file

Organized into major sections:
1. **Tracing implementation** (lines 23-66) - CODEX_TRACE logging
2. **Utility functions** (lines 68-105) - String/path helpers
3. **Overlay management** (lines 106-500) - Serialization/deserialization, mount/save
4. **Binary I/O helpers** (lines 598-690) - Binary reader/writer for serialization
5. **AST serialization** (lines 690-1283) - S-expression and C++ AST serialization
6. **Terminal handling** (lines 1283-1577) - Raw terminal mode for interactive input
7. **Command pipeline** (lines 1578-1863) - Command parsing and execution
8. **Value implementation** (lines 1863-1927) - Value::show, AST constructors, eval
9. **VFS implementation** (lines 1928-2307) - Core VFS operations
10. **S-expression parser** (lines 2308-2391) - Lexer and parser
11. **Builtins** (lines 2392-2534) - S-expression builtin functions
12. **Exec utilities** (lines 2535-2586) - External command execution
13. **C++ AST implementation** (lines 2588-2802) - C++ code generation
14. **AI bridge** (lines 2803-3202) - OpenAI and Llama integration
15. **REPL and main** (lines 3203-end) - Interactive shell and entry point

**snippet_catalog.h/cpp** - Snippet management system for AI prompts

**snippets/** - Directory containing reusable prompt snippets

### Future Modularization

The codebase is currently monolithic but well-organized with clear section boundaries. Future refactoring should split into:

- **utils.h/cpp** - String/path/exec utilities
- **vfs.h/cpp** - VFS core (VfsNode hierarchy, Vfs class)
- **value.h/cpp** - Value type and S-expression AST nodes
- **parser.h/cpp** - S-expression lexer and parser
- **builtins.h/cpp** - Builtin function implementations
- **cpp_ast.h/cpp** - C++ AST nodes and code generation
- **ai_bridge.h/cpp** - AI integration layer
- **overlay.h/cpp** - Overlay serialization/deserialization
- **terminal.h/cpp** - Terminal handling
- **shell.h/cpp** - REPL and command pipeline
- **codex.h/cpp** - Main coordination and entry point

## Key Architectural Concepts

### VFS Design

The Virtual File System uses a unified node hierarchy where:
- `VfsNode` is the base class for all nodes
- `DirNode` contains children (std::map<string, shared_ptr<VfsNode>>)
- `FileNode` stores string content
- `AstNode` extends VfsNode - **AST nodes ARE VFS nodes**

This design enables:
- Uniform tree traversal (`tree /`)
- AST nodes live in VFS directories (e.g., `/ast/expr`, `/astcpp/demo`)
- Overlay system can snapshot and restore both code and AST

### Overlay System

Multiple persistent VFS overlays can coexist:
- Base overlay (id=0) is always mounted
- Additional overlays can be mounted from files
- Path resolution checks all overlays based on policy (manual/oldest/newest)
- Dirty tracking triggers save prompts on exit
- Serialization format: text-based with "# codex-vfs-overlay 2" header

### S-Expression Language

Minimal λ-calculus with:
- Types: int, bool, string, list, closure, builtin
- Single-argument lambdas (multi-arg deferred)
- Builtins: arithmetic (+, -, *, /), comparison (=, <), list ops, string ops
- Special forms: if, lambda
- Evaluator uses Env (environment) for lexical scoping

### C++ AST Builder

Programmatic C++ code generation:
- `CppTranslationUnit` contains includes and functions
- `CppFunction` has parameters and a body (CppCompound)
- `CppCompound` is a statement block (also a VfsNode with children!)
- Statements: CppVarDecl, CppExprStmt, CppReturn, CppRangeFor
- Expressions: CppId, CppString, CppInt, CppCall, CppBinOp, CppStreamOut
- `cpp.dump` serializes to valid C++ source code

Shell commands like `cpp.func /astcpp/demo main int` create these nodes in VFS.

### AI Bridge

Supports two backends:
1. **OpenAI** - Uses Responses API with streaming
   - Configured via OPENAI_API_KEY, OPENAI_MODEL, OPENAI_BASE_URL
2. **Llama** - Local llama.cpp server
   - Configured via LLAMA_BASE_URL, LLAMA_MODEL

The `ai <prompt>` command posts to the configured backend and returns response.
`tools` command generates a summary of available shell commands for AI context.

### Command Shell

Interactive REPL with:
- Basic commands: ls, tree, mkdir, touch, cat, echo, rm, mv, link
- S-expression: parse, eval
- C++ AST: cpp.tu, cpp.include, cpp.func, cpp.param, cpp.vardecl, cpp.expr, cpp.stmt, cpp.print, cpp.return, cpp.returni, cpp.rangefor, cpp.dump
- Overlay: overlay.mount, overlay.save, overlay.list, overlay.unmount, overlay.use, overlay.policy
- Mounts: mount, mount.lib, mount.remote, mount.list, mount.allow, mount.disallow, unmount
- Solution: solution.save
- AI: ai, tools
- Utility: help, export, exit

Commands can be piped through stdin for automation.

### Remote VFS Mounting

Enables network-based VFS access between codex instances:
- **RemoteNode** - Client-side VFS node communicating via TCP sockets
- **Daemon Mode** - Server mode accepting remote mount connections (`--daemon <port>`)
- **EXEC Protocol** - Line-based protocol executing commands on remote server
- Commands: `mount.remote <host> <port> <remote-path> <local-path>`

Use case: Copy files between real filesystems on different hosts through VFS layer.
See [docs/REMOTE_VFS.md](../docs/REMOTE_VFS.md) for architecture and usage details.

### Terminal Handling

`RawTerminalMode` RAII wrapper for terminal control:
- Disables canonical mode and echo for char-by-char input
- Enables Ctrl+U (clear line), Ctrl+K (clear to end), Ctrl+C (interrupt)
- F3 bound to solution save in interactive mode
- Restores terminal state on destruction

## Code Organization Principles

1. **Section markers** - Code uses `// ====== Section ======` for navigation
2. **TRACE macros** - TRACE_FN, TRACE_MSG, TRACE_LOOP for debugging (opt-in with -DCODEX_TRACE)
3. **Anonymous namespaces** - Static helpers in anonymous namespace to avoid linkage conflicts
4. **RAII patterns** - RawTerminalMode, ScopedCoutCapture use RAII
5. **Shared pointers** - VfsNode tree uses shared_ptr, parent uses weak_ptr to avoid cycles
6. **Error handling** - Runtime errors with descriptive messages, minimal recovery

## Build System

**Makefile** targets:
- `make` / `make all` - Build codex binary (gnu++17, -O2)
- `make debug` - Rebuild with -O0 -g
- `make release` - Rebuild with -O3 -DNDEBUG
- `make sample` - Run hello-world pipeline test
- `make clean` - Remove binaries and build artifacts

Add `-DCODEX_TRACE` to CXXFLAGS to enable tracing to codex_trace.log.

## Testing

**test_harness.py** - Automated test runner
- Reads .sexp test files from tests/
- Supports OpenAI and Llama targets
- Validates AI responses and command execution
- Uses AI response caching for efficiency

Run tests:
```bash
python tools/test_harness.py                    # all tests
python tools/test_harness.py --target openai    # OpenAI only
python tools/test_harness.py --target llama -v  # Llama verbose
```

## Development Workflow

1. Modify code in VfsShell/codex.cpp or VfsShell/codex.h
2. Rebuild: `make clean && make`
3. Test manually: `./codex` or `make sample`
4. Run test suite: `python tools/test_harness.py`
5. Commit changes after successful tests

For tracing:
```bash
make clean
make CXXFLAGS="-O0 -g -DCODEX_TRACE"
printf 'ls /\ntree /\nexit\n' | ./codex
cat codex_trace.log
```

## Extension Points

To add new features:

1. **New shell command**: Add to command dispatcher in REPL section
2. **New builtin**: Add to install_builtins() in Builtins section
3. **New C++ AST node**: Derive from CppNode, implement dump(), add helper command
4. **New VFS node type**: Derive from VfsNode, handle in overlay serialization
5. **New AI provider**: Add to call_ai() dispatch logic in AI bridge section

## Known Issues & Limitations

1. Lambda expressions are single-argument only
2. String escaping in C++ AST needs verification
3. No persistence by default (requires overlay.save)
4. Minimal error recovery in parser and eval
5. Terminal handling is Unix-specific (termios)

## See Also

- [../AGENTS.md](../AGENTS.md) - High-level agent architecture
- [../README.md](../README.md) - Build and usage instructions
- [../TASKS.md](../TASKS.md) - Development roadmap
- [../DISCUSSION.md](../DISCUSSION.md) - Design discussions
