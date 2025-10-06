# Agent Stack

## Stage1 – codex-mini bootstrapper

**Mission.** Spin up a single-binary, meta-compiler-flavoured Codex assistant written in modern C++. The binary hosts an in-memory virtual file system (VFS) whose AST nodes inherit directly from VFS nodes. A tiny shell command set provides just enough power to be Turing complete (λ-calculus core + basic arithmetic and branching), matching the intent from the discussion.

**Key features.**
- Embedded VFS with `/`, `/src`, `/ast`, `/env`, `/astcpp`, `/cpp`; directories, regular files, and AST nodes share the same hierarchy.
- Lisp/S-expression toolchain: `parse` turns source files into AST holders, `eval` runs a minimal λ-language (ints, bools, strings, lists, closures, builtins `+ - * = < print`, `if`, list & string ops).
- Shell façade: `ls`, `tree`, `mkdir`, `touch`, `cat`, `echo`, `rm`, `mv`, `link`, plus help/exit. Mirrors the “tiny shell comparable to classic commands” requirement.
- C++ code builder: `cpp.tu`, `cpp.include`, `cpp.func`, `cpp.param`, `cpp.print`, `cpp.returni`, `cpp.dump` manipulate a C++ AST stored in the VFS and emit compilable code.
- AI bridge: `ai <prompt>` posts to OpenAI Responses API using environment variables (`OPENAI_API_KEY`, optional `OPENAI_MODEL`, `OPENAI_BASE_URL`). `tools` echoes the active command surface for the assistant.

**Implementation notes.**
- Source lives in `Stage1/codex.cpp` and `Stage1/codex.h`; build with `c++ -std=gnu++17 -O2 Stage1/codex.cpp -o codex`.
- `VfsNode` is the base class; `DirNode`, `FileNode`, and every AST node derive from it. `AstHolder` lets parsed expressions sit inside the VFS tree.
- Parser accepts atoms (ints, bools, strings, symbols) and list forms; current `lambda` path handles a single parameter, matching the initial scope.
- Builtins extend to list/string helpers (`list`, `cons`, `head`, `tail`, `null?`, `str.cat`, `str.sub`, `str.find`) so the evaluation core can express non-trivial programs.
- C++ AST layer models translation units, includes, functions, params, statements, and handles stream output/return generation before dumping into `/cpp`.

**Open edges & cautions.**
- String escaping in the C++ AST dumper requires a sanity check (discussion noted escape regressions in `CppString::esc` and related dumping helpers).
- `lambda` is currently single-argument; multi-parameter or variadic support is deferred.
- Error handling is minimal; shell commands throw runtime errors that bubble to the REPL.
- No persistence: everything lives in-memory inside the VFS.

**Next moves.** (See `TASKS.md` for live tracking and to kick off work with `continue`.)
1. Harden the C++ AST string escaping and scan dumps for malformed literals.
2. Broaden the λ-syntax (multi-arg lambdas, let-binding sugar) to reduce ceremony in larger programs.
3. Design Stage2 agent goals (e.g. richer codegen, persistence, higher-level scripting) on top of the Stage1 substrate.
