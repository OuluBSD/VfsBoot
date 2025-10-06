# Tasks Tracker

## Completed
- `AGENTS.md` drafted from discussion notes to document Stage1 agent scope.

## In Progress / Upcoming
- Build tooling pipeline:
  - Add a `Makefile` for compiling `Stage1/codex.cpp` (and future components).
  - Extend the app to integrate the OpenAI Platform API, loading the security token from `~/openai-key.txt` (do not hard-code; read at runtime, guard missing-file cases).
  - Create sample C++ programs through the VFS/C++ AST tooling, dump them to real files, compile, and run smoke tests via the Makefile.

## Backlog / Ideas
- Harden string escaping in the C++ AST dumper before expanding code generation.

