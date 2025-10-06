# Tasks Tracker

## Completed
- `AGENTS.md` drafted from discussion notes to document Stage1 agent scope.
- Build tooling pipeline now operational:
  - Root `Makefile` builds the Stage1 binary and exposes debug/release toggles.
  - OpenAI integration loads the API key from the home directory fallback (`~/openai-key.txt`).
  - `make sample` exercises the C++ AST builder, exports generated code, compiles it, and checks the runtime output.

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.
