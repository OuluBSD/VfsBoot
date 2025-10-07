# Claude Instructions for VfsBoot

## Project Overview

**VfsBoot** is a single-binary, meta-compiler-flavoured Codex assistant written in modern C++. It features an in-memory virtual file system (VFS) where AST nodes inherit directly from VFS nodes, creating a unified tree for code manipulation and generation.

## Key Documentation

- **[AGENTS.md](AGENTS.md)** – Detailed technical documentation for VfsShell architecture, implementation notes, and agent specifications. **Read this first for technical details.**
- **[README.md](README.md)** – Build instructions, test harness usage, overlay system, solutions, and C++ builder surface
- **[TASKS.md](TASKS.md)** – Live task tracker with prioritized features and completed work
- **[DISCUSSION.md](DISCUSSION.md)** – Design discussions and architectural decisions

## Quick Start

### Building
```sh
make            # builds ./codex with gnu++17 + O2
make debug      # -O0 -g
make release    # -O3 -DNDEBUG
make sample     # runs hello-world pipeline test
```

### Testing
```sh
python tools/test_harness.py                    # run all tests
python tools/test_harness.py --target openai    # OpenAI only
python tools/test_harness.py --target llama -v  # Llama with verbose output
```

## Architecture Essentials

### VFS Structure
- `/` – root
- `/src` – source files
- `/ast` – parsed AST nodes
- `/env` – environment variables
- `/astcpp` – C++ AST builder workspace
- `/cpp` – generated C++ code output

All nodes (directories, files, AST nodes) inherit from `VfsNode` base class.

### File Types & Conventions
- `.sexp` – S-expression scripts for AI and automation (AI-first interface)
- `.cx` – Shell scripts for human users (can be called from sexp)
- `.cxpkg` – Solution package overlay files
- `.cxasm` – Assembly overlay files

**Philosophy**: sexp is for AI and shell script is for human user (+ AI called via sexp). This follows classic codex behavior.

### Core Systems

**Shell Commands**: `ls`, `tree`, `mkdir`, `touch`, `cat`, `echo`, `rm`, `mv`, `link`, `help`, `exit`

**Lisp/S-expression toolchain**:
- `parse` – turns source into AST holders
- `eval` – runs minimal λ-language (ints, bools, strings, lists, closures, builtins)

**C++ Code Builder**: `cpp.tu`, `cpp.include`, `cpp.func`, `cpp.param`, `cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.print`, `cpp.return`, `cpp.returni`, `cpp.rangefor`, `cpp.dump`

**AI Bridge**: `ai <prompt>` posts to OpenAI/Llama; `tools` echoes command surface

**Overlay System**: Multiple persistent VFS overlays with `overlay.mount`, `overlay.save`, `overlay.list`, `overlay.unmount`, `overlay.use`, `overlay.policy`

## Important Workflows

### Code Modification Process
1. Read and understand existing code structure
2. Make targeted changes using appropriate tools
3. Test changes with the test harness
4. **Always commit changes to git after successful modifications** (see AGENTS.md Process rule)

### Task Tracking
- TASKS.md is the live task tracker
- Organized by priority: "Upcoming: important", "Upcoming: less important", "Completed", "Backlog / Ideas"
- Update TASKS.md when completing work or discovering new requirements

### File Organization
- Source lives in `VfsShell/codex.cpp` and `VfsShell/codex.h`
- Scripts organized under `scripts/` in: `examples/`, `reference/`, `unittst/`, `tutorial/`
- Tests in `tests/` directory (numbered `.sexp` files)
- Build artifacts in `build/`

## Environment Variables

### OpenAI
- `OPENAI_API_KEY` (required, fallback: `~/openai-key.txt`)
- `OPENAI_MODEL` (optional)
- `OPENAI_BASE_URL` (optional)

### Llama
- `LLAMA_BASE_URL` / `LLAMA_SERVER` (default: `http://192.168.1.169:8080`)
- `LLAMA_MODEL` (default: `coder` matching `qwen2.5-coder-7b-instruct-q4_k_m.gguf`)

### General
- `CODEX_AI_PROVIDER` – force `openai` or `llama`

## Tracing & Debugging

Build with `-DCODEX_TRACE` to enable logging to `codex_trace.log`:
```sh
make debug CXXFLAGS="-O0 -g -DCODEX_TRACE"
printf 'ls /\nls /cpp\nexit\n' | ./codex
cat codex_trace.log
```

Tracing macros: `TRACE_FN`, `TRACE_LOOP`, `TRACE_MSG`

## Critical Notes

1. **No persistence by default** – Everything lives in-memory unless using overlays
2. **Lambda is single-argument** – Multi-parameter support deferred
3. **Minimal error handling** – Shell commands throw runtime errors
4. **String escaping** – C++ AST dumper requires sanity checks (known issue)
5. **Git workflow** – Commit after every successful code modification session

## Current Focus Areas (from TASKS.md)

- Rename VfsShell completed
- Split monolithic code into multiple clean files
- Add examples for script directory usage (HOWTO_SCRIPTS.md)
- Fix test_harness.py to use compiled output verification instead of AST comparison
- Add persistent VFS file support with git-friendly diffs
- Implement action planner core engine for context management

## Next Steps for New Contributors

1. Read AGENTS.md for technical architecture
2. Build and run `make sample` to see the pipeline
3. Review existing tests in `tests/` directory
4. Check TASKS.md for current priorities
5. Explore scripts in `scripts/examples/` for usage patterns

---

For detailed agent specifications and implementation notes, see **[AGENTS.md](AGENTS.md)**.
