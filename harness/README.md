# Scenario Harness

The scenario harness provides automated testing and training data generation for the VfsBoot planner system.

## Components

### Core Files

- **scenario.h/cpp**: Scenario file parser and data structures
- **runner.h/cpp**: Scenario execution engine and verification
- **demo.cpp**: Interactive scenario runner (`planner_demo` binary)
- **train.cpp**: Batch training data generator (`planner_train` binary)

### Data Structures

#### Scenario
Represents a reproducible test case with:
- `name`: Scenario identifier
- `description`: Human-readable description
- `setup_commands`: VFS initialization commands
- `user_intent`: What the user wants to accomplish
- `expected_plan`: Expected planner decomposition
- `expected_actions`: Commands that should be executed
- `verification_checks`: Final state assertions

#### ScenarioRunner
Executes scenarios through five phases:
1. **Setup**: Initialize VFS state
2. **Plan Generation**: Process user intent through AI planner (✓ integrated)
3. **Plan Verification**: Compare actual vs expected plan
4. **Action Execution**: Execute planned commands (currently stubbed)
5. **Verification**: Assert final VFS state

#### BreakdownLoop
Iterative validation with snapshot rollback:
- Runs scenario up to max_iterations times
- Creates VFS snapshot before each attempt
- Restores snapshot on failure and retries
- Collects metrics for training feedback

## Scenario File Format

Example (`scenarios/basic/hello-world.scenario`):

```
name: hello-world
description: Simple C++ hello world generation

[SETUP]
mkdir /src
mkdir /cpp

[USER_INTENT]
Create a simple hello world program

[EXPECTED_PLAN]
1. Create translation unit
2. Add iostream include
3. Create main function
4. Add print statement
5. Add return statement

[ACTIONS]
cpp.tu /astcpp/demo
cpp.include /astcpp/demo iostream 1
cpp.func /astcpp/demo main int
cpp.print /astcpp/demo/main "Hello World"
cpp.returni /astcpp/demo/main 0
cpp.dump /astcpp/demo /cpp/demo.cpp

[VERIFICATION]
exists /cpp/demo.cpp
contains /cpp/demo.cpp "Hello World"
```

## Usage

### planner_demo
Run a single scenario interactively:
```bash
./planner_demo -v scenarios/basic/hello-world.scenario
./planner_demo -i 20 scenarios/complex/refactoring.scenario
```

Options:
- `-v, --verbose`: Enable verbose output
- `-i, --iterations N`: Max breakdown iterations (default: 10)
- `-h, --help`: Show help

### planner_train
Generate training data from multiple scenarios:
```bash
./planner_train -v -o training.json scenarios/
```

Options:
- `-v, --verbose`: Enable verbose output
- `-i, --iterations N`: Max iterations per scenario (default: 10)
- `-o, --output FILE`: Output file (default: training_data.json)

Output format (JSON):
```json
{
  "training_data": [
    {
      "scenario_name": "hello-world",
      "user_intent": "Create a simple hello world program",
      "generated_plan": "...",
      "success": true,
      "iterations": 1,
      "error_message": ""
    }
  ]
}
```

## Build Status

**Status**: ✓ COMPLETE - Both binaries build and run successfully

**Solution**: Main function wrapped with `#ifndef CODEX_NO_MAIN / #endif` in VfsShell/codex.cpp

**Build targets**:
```bash
# Build library object
x86_64-pc-linux-gnu-g++ -DCODEX_NO_MAIN -c VfsShell/codex.cpp -o build/codex_lib.o

# Build planner_demo (908K)
x86_64-pc-linux-gnu-g++ harness/demo.cpp build/codex_lib.o build/scenario.o build/runner.o \
    VfsShell/snippet_catalog.cpp VfsShell/utils.cpp -o planner_demo \
    -lblake3 -lsvn_delta-1 -lsvn_subr-1 -lexpat -lz -lmagic -llz4 -lutf8proc -laprutil-1 -lapr-1

# Build planner_train (934K)
x86_64-pc-linux-gnu-g++ harness/train.cpp build/codex_lib.o build/scenario.o build/runner.o \
    VfsShell/snippet_catalog.cpp VfsShell/utils.cpp -o planner_train \
    -lblake3 -lsvn_delta-1 -lsvn_subr-1 -lexpat -lz -lmagic -llz4 -lutf8proc -laprutil-1 -lapr-1
```

## Implementation Notes

### Current Limitations

1. **Plan Generation**: ✓ COMPLETE - Integrated with actual AI planner
   - Calls `call_ai()` with planning prompts
   - Supports both OpenAI and Llama providers
   - Language configurable via `CODEX_ENGLISH_ONLY` environment variable
   - Known issue: Plan verification uses exact text matching (too strict for AI-generated plans)

2. **Action Execution**: Currently stubbed out
   - TODO: Execute cpp.* commands through shell dispatcher
   - Requires integrating with full command loop

3. **VFS API**: Uses basic resolve() and touch() methods
   - Works for simple verification
   - Complex commands need shell integration

### Integration Points

The harness is designed to integrate with:
- **ScopeStore**: Snapshot/restore for iterative refinement
- **AI Planner**: Actual plan generation from user intent
- **Shell Commands**: cpp.*, parse, eval, etc.
- **Feature Masks**: Enable/disable functionality per scenario

### Testing Workflow

1. Create .scenario file describing test case
2. Run with `planner_demo -v` to debug interactively
3. Iterate on scenario until it passes
4. Add to training set for `planner_train`
5. Generate training data for planner improvements

## Next Steps

1. ~~Fix build system (resolve main() conflict)~~ ✓ COMPLETE
2. ~~Integrate actual AI planner for plan generation~~ ✓ COMPLETE
3. Improve plan verification: Use semantic matching instead of exact text comparison
4. Execute actions through shell command dispatcher
5. Create comprehensive scenario library
6. Implement feedback loop for planner training

## AI Planner Integration (2025-10-09)

The scenario harness now calls the actual AI planner instead of returning stubbed plans.

**Implementation**: `harness/runner.cpp:executePlanGeneration()`
- Constructs planning prompt with user intent
- Calls `call_ai()` to generate plan
- Handles errors and validates non-empty response

**Language Support**: `VfsShell/codex.cpp:system_prompt_text()`
- Auto-detects language from `LANG` environment variable
- Supports `CODEX_ENGLISH_ONLY=1` to force English responses
- Defaults to Finnish if `fi_FI` locale detected

**Testing**:
```bash
# Test with English mode
CODEX_ENGLISH_ONLY=1 ./planner_demo scenarios/basic/simple-file-creation.scenario --verbose

# Test with default (auto-detect from LANG)
./planner_demo scenarios/basic/simple-file-creation.scenario --verbose
```

**Example Output**:
```
User Intent: Create a text file with hello world content
Calling AI planner...
Generated Plan:
AI: ### Plan to Create a Text File with "Hello World" Content

1. **Create Plan Directory**:
   - Create a directory to store the plan.

2. **Create Plan Nodes**:
   - Node for the overall task.
   - Node for creating the text file.
   - Node for writing "Hello World" to the file.
...
```
