#ifndef _VfsBoot_harness_runner_h_
#define _VfsBoot_harness_runner_h_

#include "../VfsShell/codex.h"
#include "scenario.h"
#include <memory>
#include <vector>
#include <string>
#include <chrono>

namespace harness {

// Forward declarations
struct Scenario;
struct BreakdownResult;

// Scenario runner - executes scenarios and collects results
class ScenarioRunner {
public:
    ScenarioRunner(Vfs& vfs, ScopeStore& store, MetricsCollector* metrics = nullptr);

    void setVerbose(bool v);
    void setMetricsCollector(MetricsCollector* metrics);
    bool runScenario(const Scenario& scenario);

    Vfs& getVfs() { return vfs_; }
    const Vfs& getVfs() const { return vfs_; }

private:
    Vfs& vfs_;
    ScopeStore& scope_store_;
    MetricsCollector* metrics_collector_;
    bool verbose_;

    // Timing support for performance metrics
    std::chrono::steady_clock::time_point start_time_;

    bool runSetup(const Scenario& scenario);
    bool executePlanGeneration(const Scenario& scenario, std::string& plan_out);
    bool verifyPlan(const Scenario& scenario, const std::string& actual_plan);
    bool executeActions(const Scenario& scenario);
    bool verifyFinalState(const Scenario& scenario);

    std::vector<std::string> tokenizeCommand(const std::string& cmd);
};

// Breakdown result
struct BreakdownResult {
    bool success;
    size_t iterations;
    std::string error_message;
};

// Breakdown loop - validates planner decomposition
class BreakdownLoop {
public:
    BreakdownLoop(ScenarioRunner& runner, ScopeStore& store, MetricsCollector* metrics = nullptr);

    void setMaxIterations(size_t n);
    void setMetricsCollector(MetricsCollector* metrics);
    BreakdownResult run(const Scenario& scenario);
    std::string generateFeedback(const BreakdownResult& result);

private:
    ScenarioRunner& runner_;
    ScopeStore& scope_store_;
    MetricsCollector* metrics_collector_;
    size_t max_iterations_;
};

} // namespace harness

#endif
