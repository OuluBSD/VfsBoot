#include "scenario.h"
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace harness {

Scenario Scenario::parse(const std::string& content) {
    Scenario s;
    std::istringstream stream(content);
    std::string line;
    enum class Section {
        None,
        Setup,
        UserIntent,
        ExpectedPlan,
        Actions,
        Verification
    };
    Section current_section = Section::None;

    while (std::getline(stream, line)) {
        // Trim whitespace
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        };
        trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Section headers
        if (line == "[SETUP]") {
            current_section = Section::Setup;
            continue;
        } else if (line == "[USER_INTENT]") {
            current_section = Section::UserIntent;
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
        }

        // Handle metadata
        if (line.find("name:") == 0) {
            s.name = line.substr(5);
            trim(s.name);
            continue;
        } else if (line.find("description:") == 0) {
            s.description = line.substr(12);
            trim(s.description);
            continue;
        }

        // Add content to appropriate section
        switch (current_section) {
            case Section::Setup:
                s.setup_commands.push_back(line);
                break;
            case Section::UserIntent:
                if (!s.user_intent.empty()) {
                    s.user_intent += "\n";
                }
                s.user_intent += line;
                break;
            case Section::ExpectedPlan:
                if (!s.expected_plan.empty()) {
                    s.expected_plan += "\n";
                }
                s.expected_plan += line;
                break;
            case Section::Actions:
                s.expected_actions.push_back(line);
                break;
            case Section::Verification:
                s.verification_checks.push_back(line);
                break;
            case Section::None:
                // Ignore content outside sections
                break;
        }
    }

    if (s.name.empty()) {
        throw std::runtime_error("Scenario missing name field");
    }

    return s;
}

} // namespace harness
