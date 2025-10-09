# Scenario Harness Binaries

## Overview

The Scenario Harness provides standalone binaries for testing and training the planner system through reproducible scenarios. These binaries run outside the main REPL to enable automated validation, training data collection, and performance benchmarking.

## Architecture

### Binary Targets

#### 1. `planner_demo` - Interactive Demonstration
Demonstrates planner capabilities through curated scenarios.

```cpp
// Usage: ./planner_demo [scenario_file]
// Example: ./planner_demo scenarios/hello-world.scenario

struct ScenarioDemo {
    // Load and execute scenario
    void runScenario(const std::string& scenario_file);

    // Display planner state
    void visualizePlanTree();
    void showContextBuilder();
    void dumpSnapshots();

    // Interactive mode
    void stepThrough();  // Execute plan step-by-step
    void rewind();       // Revert to previous snapshot
    void replay();       // Replay from beginning
};
```

#### 2. `planner_train` - Training Data Generation
Generates training datasets from scenario execution.

```cpp
// Usage: ./planner_train --scenarios dir/ --output train.jsonl
// Example: ./planner_train --scenarios training/ --output data/train.jsonl

struct ScenarioTrainer {
    // Batch scenario execution
    void runScenarios(const std::vector<std::string>& scenarios);

    // Data collection
    struct TrainingExample {
        uint64_t snapshot_id;
        std::string user_intent;
        std::string context;
        std::string plan_output;
        std::vector<std::string> actions_taken;
        bool success;
        std::string error_message;
    };

    void collectExamples(const std::string& output_file);
    void exportJSONL(const std::string& path);
    void exportBinary(const std::string& path);
};
```

### Scenario File Format

`.scenario` files define reproducible test cases:

```
# hello-world.scenario
VERSION: 1
DESCRIPTION: Create and compile hello world program

[SETUP]
vfs.clear
scope.create "Initial state"

[USER_INTENT]
Create a C++ hello world program

[EXPECTED_PLAN]
plan.create "hello_world"
plan.goto /hello_world
plan.jobs.add "Create main function"
plan.jobs.add "Add hello world output"
plan.jobs.add "Compile and test"

[ACTIONS]
cpp.tu /astcpp/hello
cpp.func /astcpp/hello main int
cpp.print /astcpp/hello/main "Hello World"
cpp.returni /astcpp/hello/main 0
cpp.dump /astcpp/hello /cpp/hello.cpp

[VERIFICATION]
vfs.exists /cpp/hello.cpp
content.contains /cpp/hello.cpp "Hello World"
scope.create "After hello world"

[SNAPSHOTS]
snapshot_1: Initial state
snapshot_2: After hello world

[EXPECTED_DIFF]
added_paths: [/cpp/hello.cpp, /astcpp/hello, /astcpp/hello/main]
modified_paths: []
removed_paths: []
```

## Scripted Breakdown Loop

The breakdown loop validates planner decomposition across complexity levels:

```cpp
struct BreakdownLoop {
    enum class ComplexityLevel {
        Simple,      // 1-2 steps (hello world)
        Moderate,    // 3-5 steps (error handling)
        Complex,     // 6-10 steps (refactoring)
        Advanced,    // 11-20 steps (feature addition)
        Expert       // 20+ steps (architecture)
    };

    // Execute breakdown validation
    struct BreakdownResult {
        ComplexityLevel level;
        std::string user_intent;
        size_t steps_taken;
        bool decomposition_correct;
        bool execution_successful;
        double time_elapsed;
        std::vector<std::string> errors;
    };

    std::vector<BreakdownResult> validateBreakdown(
        const std::vector<std::string>& scenarios
    );

    // Metrics
    double accuracyByLevel(ComplexityLevel level) const;
    double averageSteps(ComplexityLevel level) const;
    void generateReport(const std::string& output_path);
};
```

## Implementation Plan

### Phase 1: Core Infrastructure

```cpp
// harness/scenario.h
struct Scenario {
    std::string version;
    std::string description;
    std::vector<std::string> setup_commands;
    std::string user_intent;
    std::vector<std::string> expected_plan;
    std::vector<std::string> actions;
    std::vector<std::string> verification;
    std::map<std::string, uint64_t> snapshots;

    static Scenario loadFromFile(const std::string& path);
    void execute(Vfs& vfs, ScopeStore& scope_store);
    bool verify() const;
};

// harness/runner.h
struct ScenarioRunner {
    Vfs vfs;
    ScopeStore scope_store;
    PlannerContext planner_ctx;

    void executeScenario(const Scenario& scenario);
    void captureMetrics();
    void saveResults(const std::string& output_path);
};
```

### Phase 2: Binary Targets

```makefile
# Makefile additions
HARNESS_SRC := harness/scenario.cpp harness/runner.cpp harness/breakdown.cpp
DEMO_BIN := planner_demo
TRAIN_BIN := planner_train

planner_demo: $(VFSSHELL_SRC) $(HARNESS_SRC) harness/demo.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

planner_train: $(VFSSHELL_SRC) $(HARNESS_SRC) harness/train.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
```

### Phase 3: Scenario Library

Create `scenarios/` directory with test cases:

```
scenarios/
├── basic/
│   ├── hello-world.scenario
│   ├── echo-args.scenario
│   └── read-file.scenario
├── moderate/
│   ├── error-handling.scenario
│   ├── arg-parsing.scenario
│   └── config-file.scenario
├── complex/
│   ├── refactor-duplicates.scenario
│   ├── add-logging.scenario
│   └── multi-file.scenario
├── advanced/
│   ├── feature-request.scenario
│   ├── architecture-change.scenario
│   └── performance-opt.scenario
└── expert/
    ├── migration.scenario
    ├── framework-upgrade.scenario
    └── system-redesign.scenario
```

## Usage Examples

### Demo Mode

```bash
# Run single scenario with visualization
./planner_demo scenarios/basic/hello-world.scenario

# Step-through mode
./planner_demo --step scenarios/moderate/error-handling.scenario

# Replay with snapshots
./planner_demo --replay scenarios/complex/refactor-duplicates.scenario
```

### Training Mode

```bash
# Generate training data from all scenarios
./planner_train --scenarios scenarios/ --output training/all.jsonl

# Filter by complexity
./planner_train --scenarios scenarios/advanced/ --output training/advanced.jsonl

# Binary format for faster loading
./planner_train --scenarios scenarios/ --output training/all.bin --format binary
```

### Breakdown Validation

```bash
# Validate breakdown across all levels
./planner_demo --validate-breakdown scenarios/

# Generate breakdown report
./planner_demo --breakdown-report --output metrics/breakdown.html
```

## Metrics and Validation

### Collected Metrics

1. **Decomposition Accuracy**
   - Steps predicted vs actual
   - Correct action sequence
   - Proper dependency ordering

2. **Execution Success**
   - Compilation success rate
   - Runtime correctness
   - Error recovery

3. **Performance**
   - Planning time per complexity level
   - Context building time
   - Total execution time

4. **Snapshot Efficiency**
   - Diff compression ratio
   - Snapshot creation time
   - Restore time

### Validation Criteria

```cpp
struct ValidationCriteria {
    // Decomposition
    double min_accuracy = 0.90;          // 90% correct steps
    double max_overhead_steps = 1.20;    // At most 20% extra steps

    // Execution
    double min_success_rate = 0.95;      // 95% scenarios succeed
    double max_error_rate = 0.05;        // 5% tolerable errors

    // Performance
    double max_plan_time_ms = 1000.0;    // 1s max planning time
    double max_context_time_ms = 500.0;  // 500ms max context build

    // Quality
    bool requires_correct_ordering = true;
    bool requires_minimal_steps = true;
    bool requires_error_recovery = true;
};
```

## Integration with Feedback Pipeline

Scenario results feed into the feedback pipeline for rule evolution:

```cpp
struct FeedbackLoop {
    // Collect scenario results
    void ingestScenarioResults(const std::vector<BreakdownResult>& results);

    // Identify improvement areas
    std::vector<std::string> findFailurePatterns();
    std::vector<std::string> suggestNewRules();

    // Rule evolution
    void proposeRuleUpdates(LogicEngine& logic_engine);
    void stageRulePatch(const std::string& rule_name);

    // Optional AI assistance
    void requestAIFeedback(const std::string& failure_pattern);
};
```

## Output Formats

### Training Data (JSONL)

```json
{"snapshot_id": 1, "intent": "Create hello world", "context": "...", "plan": "...", "actions": [...], "success": true}
{"snapshot_id": 2, "intent": "Add error handling", "context": "...", "plan": "...", "actions": [...], "success": true}
```

### Metrics Report (HTML)

Interactive dashboard with:
- Breakdown accuracy by complexity level
- Success rate trends over time
- Performance charts (planning, execution)
- Failure analysis and patterns
- Snapshot efficiency graphs

### Binary Format

Efficient storage for large training sets:
- Protocol buffers or custom binary serialization
- Fast loading for model training
- Compressed with zstd or lz4

## Next Steps

1. Implement `Scenario` parser for `.scenario` files
2. Create `ScenarioRunner` with VFS/ScopeStore integration
3. Build `planner_demo` binary with visualization
4. Build `planner_train` binary with data export
5. Implement `BreakdownLoop` validation
6. Create initial scenario library (5 levels × 3 scenarios = 15 scenarios)
7. Generate first training dataset
8. Build metrics dashboard
