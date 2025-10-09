#include "scenario.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace harness {

// Helper: trim whitespace
static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Load scenario from file
Scenario Scenario::loadFromFile(const std::string& path) {
    Scenario scenario;
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open scenario file: " + path);
    }

    std::string line;
    enum class Section {
        None, Setup, Intent, ExpectedPlan, Actions, Verification, Snapshots, ExpectedDiff
    } current_section = Section::None;

    while (std::getline(file, line)) {
        line = trim(line);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            // Check for metadata comments
            if (line.find("# ") == 0) {
                if (scenario.description.empty()) {
                    scenario.description = line.substr(2);
                }
            }
            continue;
        }

        // Section headers
        if (line == "[SETUP]") {
            current_section = Section::Setup;
            continue;
        } else if (line == "[USER_INTENT]") {
            current_section = Section::Intent;
            continue;
        } else if (line == "[EXPECTED_PLAN]") {
            current_section = Section::ExpectedPlan;
            continue;
        } else if (line == "[ACTIONS]") {
            current_section = Section::Actions;
            continue;
        } else if (line == "[VERIFICATION]") {
            current_section = Section::Verification;
            continue;
        } else if (line == "[SNAPSHOTS]") {
            current_section = Section::Snapshots;
            continue;
        } else if (line == "[EXPECTED_DIFF]") {
            current_section = Section::ExpectedDiff;
            continue;
        }

        // VERSION line
        if (line.substr(0, 8) == "VERSION:") {
            scenario.version = trim(line.substr(8));
            continue;
        }

        // DESCRIPTION line
        if (line.substr(0, 12) == "DESCRIPTION:") {
            scenario.description = trim(line.substr(12));
            continue;
        }

        // Process section content
        switch (current_section) {
        case Section::Setup:
            scenario.setup_commands.push_back(line);
            break;

        case Section::Intent:
            if (!scenario.user_intent.empty()) {
                scenario.user_intent += " ";
            }
            scenario.user_intent += line;
            break;

        case Section::ExpectedPlan:
            scenario.expected_plan.push_back(line);
            break;

        case Section::Actions:
            scenario.actions.push_back(line);
            break;

        case Section::Verification:
            scenario.verification.push_back(line);
            break;

        case Section::Snapshots: {
            // Format: snapshot_name: description
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string name = trim(line.substr(0, colon));
                // Snapshot ID will be assigned during execution
                scenario.snapshots[name] = 0;
            }
            break;
        }

        case Section::ExpectedDiff: {
            // Format: added_paths: [path1, path2, ...]
            if (line.substr(0, 12) == "added_paths:") {
                // Parse list
                std::string paths_str = trim(line.substr(12));
                // Simple parsing - assumes comma-separated
                // TODO: Proper list parsing
            } else if (line.substr(0, 15) == "modified_paths:") {
                // Similar parsing
            } else if (line.substr(0, 14) == "removed_paths:") {
                // Similar parsing
            }
            break;
        }

        case Section::None:
            break;
        }
    }

    return scenario;
}

bool Scenario::isValid() const {
    return !version.empty() &&
           !user_intent.empty() &&
           !actions.empty();
}

// TrainingExample JSON export
std::string TrainingExample::toJSON() const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"snapshot_id\": " << snapshot_id << ",\n";
    out << "  \"user_intent\": \"" << user_intent << "\",\n";
    out << "  \"context\": \"" << context << "\",\n";
    out << "  \"plan_output\": \"" << plan_output << "\",\n";
    out << "  \"actions\": [";
    for (size_t i = 0; i < actions_taken.size(); i++) {
        if (i > 0) out << ", ";
        out << "\"" << actions_taken[i] << "\"";
    }
    out << "],\n";
    out << "  \"success\": " << (success ? "true" : "false") << ",\n";
    out << "  \"error_message\": \"" << error_message << "\",\n";
    out << "  \"time_elapsed_ms\": " << time_elapsed_ms << "\n";
    out << "}";
    return out.str();
}

std::string TrainingExample::toJSONL() const {
    std::ostringstream out;
    out << "{";
    out << "\"snapshot_id\":" << snapshot_id << ",";
    out << "\"intent\":\"" << user_intent << "\",";
    out << "\"context\":\"" << context << "\",";
    out << "\"plan\":\"" << plan_output << "\",";
    out << "\"actions\":[";
    for (size_t i = 0; i < actions_taken.size(); i++) {
        if (i > 0) out << ",";
        out << "\"" << actions_taken[i] << "\"";
    }
    out << "],";
    out << "\"success\":" << (success ? "true" : "false");
    if (!error_message.empty()) {
        out << ",\"error\":\"" << error_message << "\"";
    }
    out << "}";
    return out.str();
}

// BreakdownResult metrics
double BreakdownResult::accuracy() const {
    if (steps_expected == 0) return 0.0;
    double correct = decomposition_correct ? 1.0 : 0.0;
    return correct;
}

double BreakdownResult::stepOverhead() const {
    if (steps_expected == 0) return 0.0;
    return static_cast<double>(steps_taken) / steps_expected;
}

// ScenarioMetrics summary
void ScenarioMetrics::printSummary() const {
    std::cout << "Scenario: " << scenario_name << "\n";
    std::cout << "Success: " << (success ? "YES" : "NO") << "\n";
    std::cout << "Steps: " << steps_executed << "\n";
    std::cout << "Plan time: " << plan_time_ms << " ms\n";
    std::cout << "Execution time: " << execution_time_ms << " ms\n";
    std::cout << "Total time: " << total_time_ms << " ms\n";
    std::cout << "Snapshots: " << snapshots_created << "\n";
    std::cout << "Diff size: " << diff_size_bytes << " bytes\n";
    if (!errors.empty()) {
        std::cout << "Errors:\n";
        for (const auto& err : errors) {
            std::cout << "  - " << err << "\n";
        }
    }
}

} // namespace harness
