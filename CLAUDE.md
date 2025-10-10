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

**C++ Code Builder**: `cpp.tu`, `cpp.include`, `cpp.func`, `cpp.param`, `cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.print`, `cpp.return`, `cpp.returni`, `cpp.rangefor`, `cpp.dump`, `sample.run`

**Make Utility** (Minimal GNU Make Subset):
- Build automation with Makefile support: `make [target] [-f makefile] [-v|--verbose]`
- Features:
  - Basic rule syntax: `target: dependencies` with tab-indented commands
  - Variable substitution: `$(VAR)` and `${VAR}`
  - Automatic variables: `$@` (target), `$<` (first prerequisite), `$^` (all prerequisites)
  - Phony targets: `.PHONY` declaration
  - Timestamp-based rebuilds (filesystem + VFS)
  - Dependency graph with cycle detection
- Default Makefile location: `/Makefile` (override with `-f`)
- Default target: `all` (or first rule)

**Sample Runner** (In-Binary Pipeline):
- Build, compile, and execute demo C++ program: `sample.run [--keep] [--trace]`
- Outputs: `/logs/sample.run.out`, `/logs/sample.compile.out`, `/env/sample.status`
- Flags: `--keep` (preserve temp files), `--trace` (verbose output)

**AI Bridge**: `ai <prompt>` posts to OpenAI/Llama; `tools` echoes command surface

**Web Server** (Browser-Based Terminal):
- Start web server: `./codex --web-server [--port 8080]`
- Opens HTTP/WebSocket server with xterm.js terminal in browser
- Features:
  - Full terminal emulation with ANSI color support
  - Real-time command execution via WebSockets
  - Responsive design with modern UI
  - Multiple concurrent sessions supported
- Built with libwebsockets for lightweight performance
- Default port: 8080 (customizable with `--port`)
- Access via browser: `http://localhost:8080/`

**Overlay System**: Multiple persistent VFS overlays with `overlay.mount`, `overlay.save`, `overlay.list`, `overlay.unmount`, `overlay.use`, `overlay.policy`

**Planner System** (Fully Integrated):
- Hierarchical planning with `plan.create`, `plan.goto`, `plan.forward`, `plan.backward`
- Interactive AI discussion with `plan.discuss` and `discuss` commands
- Context management with `plan.context.add`, `plan.context.remove`, `plan.context.list`
- Job tracking with `plan.jobs.add`, `plan.jobs.complete`
- Tag-based validation with `plan.verify`, `plan.tags.infer`, `plan.validate`

**Context Builder** (AI Context Offloader):
- Build AI context within token budgets: `context.build [max_tokens]`
- Advanced features: `context.build.adv [--deps] [--dedup] [--summary=N] [--hierarchical] [--adaptive]`
- Filter by tags: `context.filter.tag <tag-name> [any|all|none]`
- Filter by path: `context.filter.path <prefix-or-pattern>`

**Hypothesis Testing System** (5 Complexity Levels):
- Level 1: Simple queries with `hypothesis.query <target> [path]`
- Level 2: Error handling analysis with `hypothesis.errorhandling <func> [style]`
- Level 3: Duplicate detection with `hypothesis.duplicates [path] [min_lines]`
- Level 4: Logging instrumentation with `hypothesis.logging [path]`
- Level 5: Architecture patterns with `hypothesis.pattern <pattern> [path]`
- Run all levels: `test.hypothesis`

**Feedback Pipeline** (Automated Rule Evolution):
- Metrics collection and analysis: `feedback.metrics.show [top_n]`, `feedback.metrics.save [path]`
- Rule patch management: `feedback.patches.list`, `feedback.patches.apply [index|all]`, `feedback.patches.reject [index|all]`
- Full feedback cycle: `feedback.cycle [--auto-apply] [--min-evidence=N]`
- Interactive review: `feedback.review`
- See **[docs/FEEDBACK_PIPELINE.md](docs/FEEDBACK_PIPELINE.md)** for details

**Logic Engine** (Tag Theorem Proving):
- Initialize rules: `logic.init`
- Infer tags via forward chaining: `logic.infer <tag> [tag...]`
- Check consistency: `logic.check <tag> [tag...]`
- Explain inference: `logic.explain <target> <source...>`
- List all rules: `logic.listrules`

**Advanced Visualization**:
- Tree with metadata: `tree.adv [path] [--sizes] [--tags] [--colors] [--depth=N] [--filter=pattern]`

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

## Static Analysis

VfsBoot includes comprehensive static analysis support using `clang-tidy` to catch potential bugs, code quality issues, and enforce best practices beyond what the compiler warnings provide.

### Running Static Analysis

The build script (`./build.sh`) automatically prompts to run static analysis after successful builds:

```sh
# Build and get prompted for static analysis
./build.sh

# Automatically run static analysis after build
./build.sh --static-analysis

# Skip static analysis prompt (for CI/automated builds)
./build.sh --no-static-analysis
```

### Manual Static Analysis

You can also run static analysis manually on individual files:

```sh
clang-tidy VfsShell/codex.cpp \
  --checks='*,-fuchsia-*,-google-*,-llvm-*,-llvmlibc-*,-altera-*,-android-*' \
  -- -std=c++17 -I/usr/lib/llvm/21/include \
  $(pkg-config --cflags libsvn_delta libsvn_subr)
```

### When to Run Static Analysis

- **After completing a task**: The build script prompts after successful builds
- **Before committing to git**: Recommended workflow after code modifications
- **During code review**: To catch issues early
- **CI/CD pipelines**: Use `--static-analysis` flag for automated checks

### Static Analysis Output

Results are saved to `.static-analysis-*.log` files for detailed review. The tool checks for:
- Potential bugs (null pointer dereferences, use-after-free, etc.)
- Code modernization opportunities (C++11/14/17 features)
- Performance issues (inefficient copies, unnecessary allocations)
- Code readability (naming conventions, magic numbers)
- Best practices (const correctness, RAII patterns)

### Installing clang-tidy

**Gentoo**: `emerge sys-devel/clang`
**Debian/Ubuntu**: `apt-get install clang-tidy`
**Fedora/RHEL**: `dnf install clang-tools-extra`
**Arch**: `pacman -S clang`

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
