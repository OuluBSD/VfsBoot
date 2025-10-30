# Agent Stack

## VfsShell – codex-mini bootstrapper

**Mission.** Spin up a single-binary, meta-compiler-flavoured Codex assistant written in modern C++. The binary hosts an in-memory virtual file system (VFS) whose AST nodes inherit directly from VFS nodes. A tiny shell command set provides just enough power to be Turing complete (λ-calculus core + basic arithmetic and branching), matching the intent from the discussion.

**Key features.**
- Embedded VFS with `/`, `/src`, `/ast`, `/env`, `/astcpp`, `/cpp`; directories, regular files, and AST nodes share the same hierarchy.
- Filesystem mounting: `mount` binds host directories into VFS paths as overlay nodes, enabling read/write access to actual filesystems.
- Library mounting: `mount.lib` loads shared libraries (.so/.dll) and exposes them as VFS directories with symbol discovery.
- Lisp/S-expression toolchain: `parse` turns source files into AST holders, `eval` runs a minimal λ-language (ints, bools, strings, lists, closures, builtins `+ - * = < print`, `if`, list & string ops).
- Shell façade: `ls`, `tree`, `mkdir`, `touch`, `cat`, `echo`, `rm`, `mv`, `link`, plus help/exit. Mirrors the "tiny shell comparable to classic commands" requirement.
- C++ code builder: `cpp.tu`, `cpp.include`, `cpp.func`, `cpp.param`, `cpp.print`, `cpp.returni`, `cpp.dump` manipulate a C++ AST stored in the VFS and emit compilable code.
- AI bridge: `ai <prompt>` posts to OpenAI Responses API using environment variables (`OPENAI_API_KEY`, optional `OPENAI_MODEL`, `OPENAI_BASE_URL`). `tools` echoes the active command surface for the assistant.

**Implementation notes.**
- Source is organized into U++ packages under `src/`: `VfsCore/`, `VfsShell/`, `Clang/`, `UppCompat/`, `Qwen/`, `Logic/`, `WebServer/`.
- Build system uses U++ `umk` (Ultimate++ make) with package dependencies defined in `.upp` files.
- **CRITICAL PACKAGE DEPENDENCY RULE**: VfsShell and ALL packages it depends on (recursively) must ONLY use U++ Core package. **VIOLATION FOUND**: CtrlCore and CtrlLib appear in "uses" sections recursively from VfsShell. This MUST be fixed:
  - Check ALL .upp files in the dependency tree from VfsShell
  - Remove CtrlCore and CtrlLib from ALL "uses" sections
  - Remove ALL CtrlCore/CtrlLib header includes
  - VfsShell is a pure console application and must not depend on GUI packages
  - Only Core is allowed in the entire dependency chain
- Build VfsShell package: `./umk_build_VfsShell.sh` (builds VfsShell and all dependencies; reference for Core/non-GUI packages).
- Quick syntax check single file: `c++ -std=gnu++17 -fsyntax-only -I. -I.. -I$HOME/Dev/ai-upp/uppsrc -DGUI -DCLANG -DDEBUG src/VfsShell/Registry.cpp`
- U++ headers location: `$HOME/Dev/ai-upp/uppsrc` (required for Core headers)
- `VfsNode` is the base class; `DirNode`, `FileNode`, `MountNode`, `LibraryNode`, and every AST node derive from it. `AstHolder` lets parsed expressions sit inside the VFS tree.
- `MountNode` delegates read/write/children operations to the host filesystem, providing transparent access to real directories and files.
- `LibraryNode` uses `dlopen`/`dlsym` to load shared libraries and expose symbols; mount control commands (`mount.allow`/`mount.disallow`) gate new mounts without affecting existing ones.
- Parser accepts atoms (ints, bools, strings, symbols) and list forms; current `lambda` path handles a single parameter, matching the initial scope.
- Builtins extend to list/string helpers (`list`, `cons`, `head`, `tail`, `null?`, `str.cat`, `str.sub`, `str.find`) so the evaluation core can express non-trivial programs.
- C++ AST layer models translation units, includes, functions, params, statements, and handles stream output/return generation before dumping into `/cpp`.
- Companion `.cx` scripts live under `scripts/`, split into `examples/`, `reference/`, `unittst/`, and `tutorial/`. Keep these directories populated with illustrative flows as features evolve, and refresh the relevant scripts whenever we touch matching functionality.

**U++ Convention Adherence** (See UPP_CONVENTION.md)
- **We are converting to Ultimate++ (U++) conventions** - This means removing `std::` types in favor of U++ equivalents
- **Type conversions**: `std::string` → `String`, `std::vector` → `Vector`, `std::map` → `VectorMap` or `ArrayMap`, `std::unordered_map` → `Index`
- **Value semantics**: Use `<<=` (pick) or explicit `clone()`/`pick()` instead of ambiguous `=`; avoid implicit copies
- **Pointer ownership**: Prefer `One<T>` for owned pointers, `Ptr<T>`/`Pte<T>` for non-owning with validity checks; minimize `std::shared_ptr` usage
- **Header structure**: Main package header includes all dependencies; individual headers use forward declarations with comments explaining deps
- **File naming**: CapitalizedWordsWithoutUnderscore (e.g., `CppAst.cpp`, not `cpp_ast.cpp`)
- **Package organization**: Each package has `.upp` file listing dependencies (`uses`) and files, with RGB color in description
- **Include guards**: Use `#ifndef _Package_Header_h_` pattern, not `#pragma once` (BLITZ compatibility)
- **Moveable types**: Declare custom types with `Moveable<T>` base for U++ container compatibility

**Open edges & cautions.**
- String escaping in the C++ AST dumper requires a sanity check (discussion noted escape regressions in `CppString::esc` and related dumping helpers).
- `lambda` is currently single-argument; multi-parameter or variadic support is deferred.
- Error handling is minimal; shell commands throw runtime errors that bubble to the REPL.
- No persistence: everything lives in-memory inside the VFS.

**Next moves.** (See `TASKS.md` for live tracking and to kick off work with `continue`.)
1. Harden the C++ AST string escaping and scan dumps for malformed literals.
2. Broaden the λ-syntax (multi-arg lambdas, let-binding sugar) to reduce ceremony in larger programs.
3. Design Stage2 agent goals (e.g. richer codegen, persistence, higher-level scripting) on top of the VfsShell substrate.

OpenAI API Key is in ~/openai-key.txt

### qwen-code Integration

For detailed documentation on the qwen-code integration, see:
- **[QWEN.md](QWEN.md)** - Complete integration guide, protocol specification, usage instructions
- **[TASK_CONTEXT.md](TASK_CONTEXT.md)** - Current implementation status and context

**All agents working on qwen integration MUST read QWEN.md first.**

Quick reference:
- C++ implementation: `VfsShell/qwen_*.{h,cpp,test.cpp}` (~3,700 lines)
- Shell command: `qwen [--attach ID] [--list-sessions] [--mode tcp] [--port 7777]`
- Server modes: stdin (default), TCP (port 7777), named pipes
- Protocol: JSON line-buffered, bidirectional stdin/stdout or TCP
- Storage: VFS `/qwen/sessions/<id>/` with metadata.json, history.jsonl, tool_groups.jsonl
- Status: Phase 5 Option A COMPLETE ✅ (real AI working end-to-end)

### Tracing playbook
- Build with `-DCODEX_TRACE` to enable scoped logging into `codex_trace.log`. Tracing primitives are `TRACE_FN` for function entry/exit, `TRACE_LOOP` for hot loop beacons, and `TRACE_MSG` for ad-hoc notes; they auto-flush into the logfile.
- When tracing, run scripted interaction via stdin (`printf 'ls /\nls /cpp\nexit\n' | ./vfsh`) so the CLI prompt is satisfied without extra tooling. The macro guards ensure no overhead in normal builds.
- Inspect `codex_trace.log` for call ordering; use standard tooling (`sort | uniq -c`) if a suspect path emits repeated lines.

### Implementation details
- See **[VfsShell/AGENTS.md](VfsShell/AGENTS.md)** for detailed implementation architecture, file structure, code organization, and extension points.

### Process rule
- After every successful code modification session, commit the changes to git.
