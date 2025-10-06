# Tasks Tracker

## Completed
- `AGENTS.md` drafted from discussion notes to document Stage1 agent scope.
- Build tooling pipeline now operational:
  - Root `Makefile` builds the Stage1 binary and exposes debug/release toggles.
  - OpenAI integration loads the API key from the home directory fallback (`~/openai-key.txt`).
  - `make sample` exercises the C++ AST builder, exports generated code, compiles it, and checks the runtime output.
- Stage1 harness (`tools/test_harness.py`) runs `.sexp` specs end-to-end against configured LLM targets and validates results inside codex-mini.
- C++ AST shell surface now includes statements (`cpp.vardecl`, `cpp.expr`, `cpp.stmt`, `cpp.return`, `cpp.rangefor`) for structural codegen beyond canned print/return helpers.

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.
