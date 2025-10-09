# Feedback Pipeline for Planner Rule Evolution

## Overview

The feedback pipeline provides automated metrics collection and rule evolution for the VfsBoot planner system. It enables the planner to learn from execution history and automatically improve its logic rules based on observed performance patterns.

## Architecture

### Components

1. **MetricsCollector** - Captures execution metrics during planner runs
2. **RulePatchStaging** - Manages proposed rule modifications before application
3. **FeedbackLoop** - Orchestrates analysis and patch generation

### Data Flow

```
Planner Execution → Metrics Collection → Pattern Analysis → Patch Generation
                                                                    ↓
Rule Updates ← Patch Application ← Human Review ← Staged Patches
```

## Core Structures

### PlannerMetrics

Tracks execution metrics for each scenario run:

- **Execution**: scenario name, timestamp, success/failure, iterations
- **Rules**: triggered rules, failed rules, rules applied
- **Performance**: execution time, context tokens, VFS nodes examined
- **Outcome**: plan matching, action completion, verification status

### RulePatch

Represents a proposed rule modification:

- **Operation Types**:
  - `Add`: Create new rules
  - `Modify`: Update existing rule formulas/confidence
  - `Remove`: Delete underperforming rules
  - `AdjustConfidence`: Fine-tune confidence values

- **Metadata**:
  - Rationale (why this change is proposed)
  - Evidence count (how many runs support this)
  - Source (`learned`, `ai-generated`, `user`)

### Pattern Detection

The feedback loop analyzes metrics to identify:

- **High-performing rules** (>90% success, ≥5 triggers) → increase confidence
- **Low-performing rules** (<50% success, ≥3 triggers) → decrease confidence
- **Failing rules** (<20% success, ≥5 triggers) → propose removal

## Shell Commands

### Metrics Commands

```bash
feedback.metrics.show [top_n]
# Display metrics summary with top triggered/failed rules
# Default: shows top 10 rules

feedback.metrics.save [path]
# Save metrics history to VFS
# Default path: /plan/metrics
```

### Patch Commands

```bash
feedback.patches.list
# List all pending rule patches with details

feedback.patches.apply [index|all]
# Apply specific patch by index, or all patches
# Example: feedback.patches.apply 0
#          feedback.patches.apply all

feedback.patches.reject [index|all]
# Reject specific patch or all patches

feedback.patches.save [path]
# Save patches to VFS (pending and applied)
# Default path: /plan/patches
```

### Cycle Commands

```bash
feedback.cycle [--auto-apply] [--min-evidence=N]
# Run complete feedback cycle:
#   1. Analyze metrics
#   2. Generate patches
#   3. Stage patches
#   4. Optionally auto-apply
#
# Options:
#   --auto-apply: Automatically apply all generated patches
#   --min-evidence=N: Minimum evidence count (default: 3)

feedback.review
# Interactive patch review interface
# Shows pending patches with detailed information
```

## Usage Workflow

### 1. Collect Metrics

Metrics are automatically collected during scenario harness runs:

```bash
./planner_demo scenarios/basic/hello-world.scenario
./planner_train scenarios/
```

### 2. Analyze and Generate Patches

Run the feedback cycle to analyze metrics:

```bash
feedback.cycle --min-evidence=3
```

This will:
- Detect performance patterns
- Generate rule patches with rationale
- Stage patches for review

### 3. Review Patches

List and review pending patches:

```bash
feedback.patches.list
feedback.review
```

Each patch shows:
- Operation type
- Rule name
- Confidence adjustment
- Rationale
- Evidence count (number of supporting runs)

### 4. Apply or Reject

Apply patches you approve:

```bash
feedback.patches.apply 0    # Apply first patch
feedback.patches.apply all  # Apply all patches
```

Reject patches you don't want:

```bash
feedback.patches.reject 0    # Reject first patch
feedback.patches.reject all  # Reject all patches
```

### 5. Persist Changes

Save metrics and patches to VFS:

```bash
feedback.metrics.save
feedback.patches.save
```

## Integration with Scenario Harness

The feedback pipeline is designed to integrate seamlessly with the scenario harness:

### In ScenarioRunner

```cpp
// Initialize metrics collection
metrics_collector.startRun(scenario.name);

// During plan execution
metrics_collector.recordRuleTrigger("rule-name");
metrics_collector.recordRuleFailed("rule-name");

// After execution
metrics_collector.recordSuccess(success, error);
metrics_collector.recordIterations(iteration_count);
metrics_collector.recordPerformance(exec_time, tokens, nodes);
metrics_collector.recordOutcome(plan_matched, actions_done, verified);

// Finish run
metrics_collector.finishRun();
```

### In BreakdownLoop

```cpp
for (size_t i = 0; i < max_iterations; ++i) {
    // Create VFS snapshot
    uint64_t snapshot_id = scope_store.createSnapshot(vfs, "iteration " + std::to_string(i));

    // Try to execute scenario
    bool success = runner.runScenario(scenario);

    if (success) {
        break;  // Success!
    }

    // Restore snapshot and retry
    scope_store.restoreSnapshot(vfs, snapshot_id);
}
```

## Advanced Features

### Optional AI Assistance

Enable AI-assisted patch generation:

```cpp
feedback_loop.use_ai_assistance = true;
feedback_loop.ai_provider = "openai";  // or "llama"
```

When enabled, the feedback loop can:
- Generate complex rule formulas
- Provide detailed rationale
- Suggest new rules based on failure patterns

### Custom Pattern Detection

Override pattern detection thresholds:

```cpp
bool FeedbackLoop::shouldIncreaseConfidence(const RulePattern& pattern) const {
    return pattern.success_rate > 0.85 && pattern.trigger_count >= 10;
}
```

### VFS Persistence Format

Metrics are stored in human-readable format:

```
/plan/metrics/history.txt:
[RUN]
scenario: hello-world
timestamp: 1696789234
success: true
iterations: 1
rules_applied: 5
rules_triggered: cached-not-remote fast-cached
execution_time_ms: 245.3
...
```

Patches are stored with full details:

```
/plan/patches/pending/pending.txt:
[PATCH 0]
operation: adjust_confidence
rule_name: fast-cached
confidence: 0.95
source: learned
rationale: High success rate: 92.5%
evidence_count: 12
```

## Performance Considerations

- **Metrics Collection**: Minimal overhead (<1% execution time)
- **Pattern Detection**: O(n) where n = number of rules
- **Patch Generation**: Lightweight, runs in milliseconds
- **VFS Persistence**: Incremental saves, only modified data

## Future Enhancements

- AI-assisted rule formula generation
- Success pattern mining (learn from what works)
- Failure pattern mining (learn from what fails)
- Automated A/B testing of rule variants
- Rule confidence decay over time
- Multi-objective optimization (success rate vs. execution time)

## Examples

See `scripts/examples/feedback-pipeline-demo.cx` for a complete working example.

## Related Documentation

- [AGENTS.md](../AGENTS.md) - Overall system architecture
- [ACTION_PLANNER.md](ACTION_PLANNER.md) - Context builder and hypothesis testing
- [SCOPE_STORE.md](SCOPE_STORE.md) - VFS snapshots and feature masks
- [harness/README.md](../harness/README.md) - Scenario harness documentation
