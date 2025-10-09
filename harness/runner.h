#ifndef _VfsBoot_harness_runner_h_
#define _VfsBoot_harness_runner_h_

#include "../VfsShell/codex.h"
#include "scenario.h"
#include <memory>
#include <vector>
#include <string>

namespace harness {

// Forward declarations
struct Scenario;
struct BreakdownResult;

// Scenario runner - executes scenarios and collects results
class ScenarioRunner {
public:
    ScenarioRunner(Vfs& vfs, ScopeStore& store);

    void setVerbose(bool v);
    bool runScenario(const Scenario& scenario);

    Vfs& getVfs() { return vfs_; }
    const Vfs& getVfs() const { return vfs_; }

private:
    Vfs& vfs_;
    ScopeStore& scope_store_;
    bool verbose_;

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
    BreakdownLoop(ScenarioRunner& runner, ScopeStore& store);

    void setMaxIterations(size_t n);
    BreakdownResult run(const Scenario& scenario);
    std::string generateFeedback(const BreakdownResult& result);

private:
    ScenarioRunner& runner_;
    ScopeStore& scope_store_;
    size_t max_iterations_;
};

} // namespace harness

#endif
