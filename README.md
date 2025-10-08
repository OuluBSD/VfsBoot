# VfsBoot
Making codex-like tool with locally hosted AI

## Building

Use the root `Makefile` to build the stage1 binary:

```sh
make            # builds ./codex with gnu++17 + O2
make debug      # rebuilt with -O0 -g
make release    # rebuilt with -O3 -DNDEBUG
```

## OpenAI key bootstrap

`codex` reads the OpenAI API token from `OPENAI_API_KEY` and falls back to `~/openai-key.txt`. Store the key as a single line in that file if you do not want to export the environment variable.

## Sample pipeline

```sh
make sample
```

`make sample` pipes scripted commands into `codex`, generating a hello-world translation unit through the C++ AST commands. The target exports the generated code to `build/demo.cpp`, compiles it with the active `CXXFLAGS`, executes the binary, and asserts that the expected greeting appears.

## VfsShell Test Harness

`tools/test_harness.py` automates the S-expression workflows defined under `tests/`. The harness loads each test-case, sends the prompt to the configured LLM targets, checks the response against the expected substrings, executes the produced command stream inside `codex`, and validates the post-conditions via `cat` assertions.

Usage examples:

```sh
python tools/test_harness.py                      # run all tests against every target declared in the .sexp files
python tools/test_harness.py --target openai      # restrict execution to OpenAI targets
python tools/test_harness.py tests/001-*.sexp     # run a subset of specs
```

Environment variables:

- `OPENAI_API_KEY` (required for OpenAI mode) plus optional `OPENAI_MODEL` and `OPENAI_BASE_URL` feed the Responses API client used by `codex` and the test harness.
- `LLAMA_BASE_URL` / `LLAMA_SERVER` point to a llama.cpp HTTP server (defaults to `http://192.168.1.169:8080` if unset). `LLAMA_MODEL` picks the hosted model name (defaults to `coder`, matching `qwen2.5-coder-7b-instruct-q4_k_m.gguf`).
- `CODEX_AI_PROVIDER` force-selects `openai` or `llama` for the VfsShell `ai` command; omit to auto-detect based on available credentials.
- The harness assumes the VfsShell binary lives at `./codex`; override with `--binary` if needed.

## Filesystem Mounting

`codex` supports mounting host filesystems and shared libraries into the VFS:

- `mount <host-path> <vfs-path>` — bind a host directory or file into the VFS, enabling transparent read/write access to real files
- `mount.lib <lib-path> <vfs-path>` — load a shared library (.so/.dll) and expose it as a VFS directory with symbol information
- `mount.list` — show all active mounts with their host paths and types (m=filesystem, l=library)
- `mount.allow` / `mount.disallow` — enable/disable mounting capability (existing mounts remain active)
- `unmount <vfs-path>` — remove a mount point

Example:
```sh
mkdir /mnt
mount tests /mnt/tests          # mount host directory
ls /mnt/tests                   # browse mounted files
mount.lib tests/libtest.so /dev/testlib  # mount shared library
cat /dev/testlib/_info          # view library metadata
```

Mount nodes (`m`) and library nodes (`l`) appear in directory listings with their special type markers. Mounted filesystems provide full read/write access to host files, while library nodes expose loaded symbols for inspection.

## Overlays

`codex` can load multiple persistent overlays simultaneously. Use `overlay.mount <name> <file>` to register a VFS snapshot (text format headed by `# codex-vfs-overlay 3`). The shell shows aggregated directory listings across matching overlays, `overlay.list` prints the active stack (`*` = primary write target, `+` = visible at the current path), and `overlay.unmount <name>` detaches an overlay (base overlay `0` is permanent). The active read/write policy can be tuned via `overlay.policy manual|oldest|newest`; under `manual` (default) ambiguous paths require `overlay.use <name>` to pick the write target, while `oldest`/`newest` resolve ties automatically. Persist a snapshot back to disk with `overlay.save <name> <file>` — directories, regular files, and AST nodes now round-trip so parsed S-expressions and C++ builder artifacts survive reloads.

## Solutions

- Solution packages are overlay files with the `.cxpkg` extension (assemblies may use `.cxasm`). Launching `codex` inside `project/` auto-loads `project.cxpkg` if it exists, or pass `--solution path/to/pkg.cxpkg` explicitly.
- Interactive sessions bind F3 (and the `solution.save [path]` command) to persist the active solution using the in-memory overlay dumper.
- On exit, codex prompts to save dirty solutions; `solution.save` updates the tracked path (handy when saving a copy).

## Extended C++ Builder Surface

The VfsShell shell now supports richer translation-unit scripting. Beyond `cpp.tu`, `cpp.include`, `cpp.func`, and `cpp.param`, you can build statements structurally:

- `cpp.vardecl <scope> <type> <name> [init]` — emit a variable declaration with optional initializer (braced, parenthesised, or `=` forms).
- `cpp.expr <scope> <expr>` — append an expression statement; `\n` sequences are unescaped before dumping.
- `cpp.stmt <scope> <raw>` — inject raw statement text (multi-line blocks supported via `\n`).
- `cpp.print <scope> <text>` — convenience helper for `std::cout << ... << std::endl;` that now targets any block.
- `cpp.return <scope> [expr]` and `cpp.returni <scope> <int>` — return from the given block.
- `cpp.rangefor <scope> <name> decl | range` — add a range-based for-loop node whose body is addressable at `<scope>/<name>/body`.

Every scope parameter may reference a function (`/astcpp/.../main`), a loop (`/astcpp/.../main/loop`), or any nested compound block (e.g. `/astcpp/.../main/body`).
