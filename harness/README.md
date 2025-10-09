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
2. **Plan Generation**: Process user intent (currently stubbed)
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

**Status**: 95% complete, pending build system refactoring

**Issue**: Both `VfsShell/codex.cpp` and `harness/demo.cpp` define `main()`, causing link-time conflict.

**Solution options**:
1. Wrap `main()` in codex.cpp with `#ifndef CODEX_LIB_ONLY`
2. Move `main()` from codex.cpp to separate `VfsShell/main.cpp`
3. Create static library from VFS/Scope/Planner code

**Build targets** (defined but not working):
```bash
make planner-demo    # Currently fails at link
make planner-train   # Currently fails at link
```

## Implementation Notes

### Current Limitations

1. **Plan Generation**: Currently returns expected_plan verbatim
   - TODO: Integrate with actual AI planner

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

1. Fix build system (resolve main() conflict)
2. Integrate actual AI planner for plan generation
3. Execute actions through shell command dispatcher
4. Create comprehensive scenario library
5. Implement feedback loop for planner training
