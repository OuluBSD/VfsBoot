#ifndef _VfsBoot_harness_runner_h_
#define _VfsBoot_harness_runner_h_

#include "../VfsShell/codex.h"
#include "scenario.h"
#include <memory>

namespace harness {

// Scenario runner - executes scenarios and collects results
class ScenarioRunner {
public:
    Vfs vfs;
    ScopeStore scope_store;
    TagRegistry& tag_registry;
    TagStorage& tag_storage;

    ScenarioRunner()
        : tag_registry(vfs.tag_registry)
        , tag_storage(vfs.tag_storage) {}

    // Execute single scenario
    ScenarioMetrics executeScenario(const Scenario& scenario);

    // Execute command from scenario
    bool executeCommand(const std::string& command);

    // Verification
    bool verifyCondition(const std::string& condition);

    // Training data collection
    TrainingExample captureTrainingExample(
        const std::string& intent,
        uint64_t snapshot_id
    );

    // Metrics
    const ScenarioMetrics& lastMetrics() const { return last_metrics; }

private:
    ScenarioMetrics last_metrics;
    std::chrono::steady_clock::time_point start_time;

    void startTimer();
    double elapsedMs() const;
};

// Breakdown loop - validates planner decomposition
class BreakdownLoop {
public:
    using ComplexityLevel = BreakdownResult::ComplexityLevel;

    BreakdownLoop(ScenarioRunner& runner) : runner(runner) {}

    // Run breakdown validation
    std::vector<BreakdownResult> validateBreakdown(
        const std::vector<std::string>& scenario_files
    );

    // Metrics by complexity level
    double accuracyByLevel(ComplexityLevel level) const;
    double averageSteps(ComplexityLevel level) const;
    double successRate(ComplexityLevel level) const;

    // Reporting
    void generateReport(const std::string& output_path);
    void printSummary() const;

private:
    ScenarioRunner& runner;
    std::vector<BreakdownResult> results;

    ComplexityLevel determineComplexity(const Scenario& scenario) const;
    bool validateDecomposition(
        const Scenario& scenario,
        const std::vector<std::string>& actual_steps
    );
};

// Training data collector
class TrainingCollector {
public:
    TrainingCollector(ScenarioRunner& runner) : runner(runner) {}

    // Collect training examples from scenarios
    void collectFromScenarios(const std::vector<std::string>& scenario_files);

    // Export formats
    void exportJSONL(const std::string& output_path);
    void exportBinary(const std::string& output_path);

    // Statistics
    size_t exampleCount() const { return examples.size(); }
    size_t successCount() const;
    size_t failureCount() const;

private:
    ScenarioRunner& runner;
    std::vector<TrainingExample> examples;
};

} // namespace harness

#endif
