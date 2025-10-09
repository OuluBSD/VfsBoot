#ifndef _VfsBoot_harness_scenario_h_
#define _VfsBoot_harness_scenario_h_

#include <string>
#include <vector>

namespace harness {

// Scenario represents a reproducible test case for the planner
struct Scenario {
    std::string name;
    std::string description;
    std::vector<std::string> setup_commands;
    std::string user_intent;
    std::string expected_plan;
    std::vector<std::string> expected_actions;
    std::vector<std::string> verification_checks;

    // Parse scenario from .scenario file content
    static Scenario parse(const std::string& content);
};

} // namespace harness

#endif
