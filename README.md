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

## Stage1 Test Harness

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
- `CODEX_AI_PROVIDER` force-selects `openai` or `llama` for the Stage1 `ai` command; omit to auto-detect based on available credentials.
- The harness assumes the Stage1 binary lives at `./codex`; override with `--binary` if needed.

## Extended C++ Builder Surface

The Stage1 shell now supports richer translation-unit scripting. Beyond `cpp.tu`, `cpp.include`, `cpp.func`, and `cpp.param`, you can build statements structurally:

- `cpp.vardecl <scope> <type> <name> [init]` — emit a variable declaration with optional initializer (braced, parenthesised, or `=` forms).
- `cpp.expr <scope> <expr>` — append an expression statement; `\n` sequences are unescaped before dumping.
- `cpp.stmt <scope> <raw>` — inject raw statement text (multi-line blocks supported via `\n`).
- `cpp.print <scope> <text>` — convenience helper for `std::cout << ... << std::endl;` that now targets any block.
- `cpp.return <scope> [expr]` and `cpp.returni <scope> <int>` — return from the given block.
- `cpp.rangefor <scope> <name> decl | range` — add a range-based for-loop node whose body is addressable at `<scope>/<name>/body`.

Every scope parameter may reference a function (`/astcpp/.../main`), a loop (`/astcpp/.../main/loop`), or any nested compound block (e.g. `/astcpp/.../main/body`).
