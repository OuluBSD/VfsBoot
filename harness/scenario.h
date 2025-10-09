#ifndef _VfsBoot_harness_scenario_h_
#define _VfsBoot_harness_scenario_h_

#include "../VfsShell/codex.h"
#include <string>
#include <vector>
#include <map>

namespace harness {

// Scenario represents a reproducible test case for the planner
struct Scenario {
    std::string version;
    std::string description;

    // Scenario sections
    std::vector<std::string> setup_commands;
    std::string user_intent;
    std::vector<std::string> expected_plan;
    std::vector<std::string> actions;
    std::vector<std::string> verification;

    // Snapshot tracking
    std::map<std::string, uint64_t> snapshots;

    // Expected results
    struct ExpectedDiff {
        std::vector<std::string> added_paths;
        std::vector<std::string> modified_paths;
        std::vector<std::string> removed_paths;
    } expected_diff;

    // Load scenario from file
    static Scenario loadFromFile(const std::string& path);

    // Validation
    bool isValid() const;
};

// Training example for ML
struct TrainingExample {
    uint64_t snapshot_id;
    std::string user_intent;
    std::string context;
    std::string plan_output;
    std::vector<std::string> actions_taken;
    bool success;
    std::string error_message;
    double time_elapsed_ms;

    // Export formats
    std::string toJSON() const;
    std::string toJSONL() const;  // Single-line JSON for training
};

// Breakdown validation result
struct BreakdownResult {
    enum class ComplexityLevel {
        Simple,      // 1-2 steps
        Moderate,    // 3-5 steps
        Complex,     // 6-10 steps
        Advanced,    // 11-20 steps
        Expert       // 20+ steps
    };

    ComplexityLevel level;
    std::string user_intent;
    size_t steps_expected;
    size_t steps_taken;
    bool decomposition_correct;
    bool execution_successful;
    double time_elapsed_ms;
    std::vector<std::string> errors;

    // Metrics
    double accuracy() const;
    double stepOverhead() const;
};

// Scenario execution metrics
struct ScenarioMetrics {
    std::string scenario_name;
    bool success;
    size_t steps_executed;
    double plan_time_ms;
    double execution_time_ms;
    double total_time_ms;
    size_t snapshots_created;
    size_t diff_size_bytes;
    std::vector<std::string> errors;

    void printSummary() const;
};

} // namespace harness

#endif
