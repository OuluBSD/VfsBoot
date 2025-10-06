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
