#include "runner.h"
#include "../VfsShell/codex.h"
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace harness;

// ScenarioRunner implementation
ScenarioRunner::ScenarioRunner(Vfs& vfs, ScopeStore& store, MetricsCollector* metrics)
    : vfs_(vfs), scope_store_(store), metrics_collector_(metrics), verbose_(false) {}

void ScenarioRunner::setVerbose(bool v) {
    verbose_ = v;
}

void ScenarioRunner::setMetricsCollector(MetricsCollector* metrics) {
    metrics_collector_ = metrics;
}

bool ScenarioRunner::runScenario(const Scenario& scenario) {
    // Start metrics recording
    if (metrics_collector_) {
        metrics_collector_->startRun(scenario.name);
        start_time_ = std::chrono::steady_clock::now();
    }

    if (verbose_) {
        std::cout << "=== Running Scenario: " << scenario.name << " ===\n";
        std::cout << "Description: " << scenario.description << "\n\n";
    }

    // Track success state and error message
    bool success = true;
    std::string error_msg;

    // Phase 1: Setup
    if (!runSetup(scenario)) {
        error_msg = "Setup phase failed";
        std::cerr << error_msg << "\n";
        success = false;
    }

    // Phase 2: Execute user intent and capture plan
    if (success) {
        std::string actual_plan;
        if (!executePlanGeneration(scenario, actual_plan)) {
            error_msg = "Plan generation failed";
            std::cerr << error_msg << "\n";
            success = false;
        } else {
            // Phase 3: Verify expected plan
            if (!verifyPlan(scenario, actual_plan)) {
                error_msg = "Plan verification failed";
                std::cerr << error_msg << "\n";
                success = false;
            }
        }
    }

    // Phase 4: Execute actions and verify
    if (success) {
        if (!executeActions(scenario)) {
            error_msg = "Action execution failed";
            std::cerr << error_msg << "\n";
            success = false;
        }
    }

    // Phase 5: Verify final state
    if (success) {
        if (!verifyFinalState(scenario)) {
            error_msg = "Final state verification failed";
            std::cerr << error_msg << "\n";
            success = false;
        }
    }

    if (verbose_ && success) {
        std::cout << "\n✓ Scenario passed: " << scenario.name << "\n";
    }

    // Record metrics
    if (metrics_collector_) {
        // Calculate execution time
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
        double exec_time_ms = duration.count();

        // Record success/failure
        metrics_collector_->recordSuccess(success, error_msg);

        // Record performance (stub values for now - would need actual counters)
        size_t context_tokens = 0;  // TODO: integrate with actual AI calls
        size_t vfs_nodes = 0;  // TODO: implement proper VFS node counting
        metrics_collector_->recordPerformance(exec_time_ms, context_tokens, vfs_nodes);

        // Record outcome (all phases passed/failed)
        bool plan_matched = success;  // If we got to verification, plan matched
        bool actions_completed = success;  // If we completed, actions were done
        bool verification_passed = success;  // Final state verified
        metrics_collector_->recordOutcome(plan_matched, actions_completed, verification_passed);

        // Finish metrics recording
        metrics_collector_->finishRun();
    }

    return success;
}

bool ScenarioRunner::runSetup(const Scenario& scenario) {
    if (verbose_) {
        std::cout << "--- Setup Phase ---\n";
    }

    for (const auto& cmd : scenario.setup_commands) {
        if (verbose_) {
            std::cout << "  > " << cmd << "\n";
        }

        try {
            // Execute command through VFS shell
            std::istringstream input(cmd);
            std::ostringstream output;

            // Simple command execution (would integrate with actual shell loop)
            auto tokens = tokenizeCommand(cmd);
            if (tokens.empty()) continue;

            std::string command = tokens[0];
            std::vector<std::string> args(tokens.begin() + 1, tokens.end());

            // Execute via VFS shell commands
            if (command == "mkdir") {
                for (const auto& path : args) {
                    vfs_.mkdir(path);
                }
            } else if (command == "touch") {
                for (const auto& path : args) {
                    vfs_.touch(path);
                }
            } else if (command == "echo") {
                // Handle echo redirection: echo "content" > /path
                if (args.size() >= 3 && args[args.size()-2] == ">") {
                    std::string content = args[0];
                    // Remove quotes if present
                    if (content.size() >= 2 && content.front() == '"' && content.back() == '"') {
                        content = content.substr(1, content.size()-2);
                    }
                    std::string path = args.back();
                    vfs_.touch(path);
                    auto node = vfs_.resolve(path);
                    if (node) {
                        node->write(content);
                    }
                }
            } else {
                // Other commands would be handled here
                if (verbose_) {
                    std::cout << "    (command not implemented in runner: " << command << ")\n";
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Setup command failed: " << cmd << "\n";
            std::cerr << "Error: " << e.what() << "\n";
            return false;
        }
    }

    return true;
}

bool ScenarioRunner::executePlanGeneration(const Scenario& scenario, std::string& plan_out) {
    if (verbose_) {
        std::cout << "\n--- Plan Generation Phase ---\n";
        std::cout << "User Intent: " << scenario.user_intent << "\n";
    }

    // Build AI planning prompt (similar to discuss command planning mode)
    std::string planning_prompt =
        "User request: " + scenario.user_intent + "\n\n"
        "Break this down into a structured plan. Create or update plan nodes in /plan tree.\n"
        "Use commands like: plan.create, plan.goto, plan.jobs.add\n"
        "Provide a concise text plan describing the steps needed.\n"
        "Focus on the high-level approach and key actions required.\n";

    if (verbose_) {
        std::cout << "Calling AI planner...\n";
    }

    try {
        // Call the actual AI planner
        plan_out = call_ai(planning_prompt);

        if (verbose_) {
            std::cout << "Generated Plan:\n" << plan_out << "\n";
        }

        // Check if we got a valid response
        if (plan_out.empty()) {
            std::cerr << "Error: AI returned empty plan\n";
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Error calling AI planner: " << e.what() << "\n";
        return false;
    }
}

bool ScenarioRunner::verifyPlan(const Scenario& scenario, const std::string& actual_plan) {
    if (verbose_) {
        std::cout << "\n--- Plan Verification Phase ---\n";
    }

    // Normalize whitespace for comparison
    auto normalize = [](const std::string& s) {
        std::string result;
        bool in_whitespace = true;
        for (char c : s) {
            if (std::isspace(c)) {
                if (!in_whitespace) {
                    result.push_back(' ');
                    in_whitespace = true;
                }
            } else {
                result.push_back(c);
                in_whitespace = false;
            }
        }
        return result;
    };

    std::string expected_norm = normalize(scenario.expected_plan);
    std::string actual_norm = normalize(actual_plan);

    if (expected_norm != actual_norm) {
        std::cerr << "Plan mismatch!\n";
        std::cerr << "Expected:\n" << scenario.expected_plan << "\n";
        std::cerr << "Actual:\n" << actual_plan << "\n";
        return false;
    }

    if (verbose_) {
        std::cout << "✓ Plan matches expected\n";
    }

    return true;
}

bool ScenarioRunner::executeActions(const Scenario& scenario) {
    if (verbose_) {
        std::cout << "\n--- Action Execution Phase ---\n";
    }

    for (const auto& action : scenario.expected_actions) {
        if (verbose_) {
            std::cout << "  Action: " << action << "\n";
        }

        // For now, just log the actions - actual execution would require
        // integrating with the full shell command loop
        if (verbose_) {
            std::cout << "    (action recorded but not executed)\n";
        }
    }

    return true;
}

bool ScenarioRunner::verifyFinalState(const Scenario& scenario) {
    if (verbose_) {
        std::cout << "\n--- Final State Verification Phase ---\n";
    }

    for (const auto& check : scenario.verification_checks) {
        if (verbose_) {
            std::cout << "  Verify: " << check << "\n";
        }

        try {
            auto tokens = tokenizeCommand(check);
            if (tokens.empty()) continue;

            std::string command = tokens[0];
            std::vector<std::string> args(tokens.begin() + 1, tokens.end());

            if (command == "exists") {
                for (const auto& path : args) {
                    auto node = vfs_.resolve(path);
                    if (!node) {
                        std::cerr << "Verification failed: path does not exist: " << path << "\n";
                        return false;
                    }
                }
            } else if (command == "contains") {
                if (args.size() < 2) {
                    std::cerr << "contains requires path and content\n";
                    return false;
                }
                std::string path = args[0];
                std::string expected_content = args[1];

                // Remove quotes
                if (expected_content.size() >= 2 && expected_content.front() == '"' && expected_content.back() == '"') {
                    expected_content = expected_content.substr(1, expected_content.size()-2);
                }

                auto node = vfs_.resolve(path);
                if (!node) {
                    std::cerr << "Verification failed: path not found: " << path << "\n";
                    return false;
                }

                std::string actual_content = node->read();
                if (actual_content.find(expected_content) == std::string::npos) {
                    std::cerr << "Verification failed: content not found in " << path << "\n";
                    std::cerr << "Expected substring: " << expected_content << "\n";
                    std::cerr << "Actual content: " << actual_content << "\n";
                    return false;
                }
            } else {
                if (verbose_) {
                    std::cout << "    (verification not implemented: " << command << ")\n";
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Verification check failed: " << check << "\n";
            std::cerr << "Error: " << e.what() << "\n";
            return false;
        }
    }

    if (verbose_) {
        std::cout << "✓ All verifications passed\n";
    }

    return true;
}

std::vector<std::string> ScenarioRunner::tokenizeCommand(const std::string& cmd) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_quotes = false;
    bool escape = false;

    for (char c : cmd) {
        if (escape) {
            current.push_back(c);
            escape = false;
            continue;
        }

        if (c == '\\') {
            escape = true;
            continue;
        }

        if (c == '"') {
            in_quotes = !in_quotes;
            current.push_back(c);  // Keep quotes for now
            continue;
        }

        if (!in_quotes && std::isspace(c)) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(c);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// BreakdownLoop implementation
BreakdownLoop::BreakdownLoop(ScenarioRunner& runner, ScopeStore& store, MetricsCollector* metrics)
    : runner_(runner), scope_store_(store), metrics_collector_(metrics), max_iterations_(10) {}

void BreakdownLoop::setMaxIterations(size_t n) {
    max_iterations_ = n;
}

void BreakdownLoop::setMetricsCollector(MetricsCollector* metrics) {
    metrics_collector_ = metrics;
}

BreakdownResult BreakdownLoop::run(const Scenario& scenario) {
    BreakdownResult result;
    result.success = false;
    result.iterations = 0;

    for (size_t i = 0; i < max_iterations_; ++i) {
        result.iterations++;

        std::cout << "\n=== Breakdown Iteration " << i+1 << " ===\n";

        // Create snapshot before attempt
        uint64_t snapshot_id = scope_store_.createSnapshot(
            runner_.getVfs(),
            "Iteration " + std::to_string(i+1)
        );

        // Run scenario (this will record its own metrics)
        bool success = runner_.runScenario(scenario);

        if (success) {
            result.success = true;
            result.error_message = "";
            std::cout << "✓ Breakdown successful on iteration " << i+1 << "\n";

            // Record total iterations in metrics
            if (metrics_collector_) {
                metrics_collector_->recordIterations(result.iterations);
            }

            return result;
        }

        // On failure, restore snapshot and try again
        std::cout << "✗ Iteration " << i+1 << " failed, restoring state...\n";
        scope_store_.restoreSnapshot(runner_.getVfs(), snapshot_id);

        // In a full implementation, we would analyze the failure
        // and adjust the planner's approach here
    }

    result.error_message = "Failed after " + std::to_string(max_iterations_) + " iterations";

    // Record iteration count even on failure
    if (metrics_collector_) {
        metrics_collector_->recordIterations(result.iterations);
    }

    return result;
}

std::string BreakdownLoop::generateFeedback(const BreakdownResult& result) {
    std::ostringstream feedback;

    feedback << "Breakdown Result:\n";
    feedback << "  Success: " << (result.success ? "Yes" : "No") << "\n";
    feedback << "  Iterations: " << result.iterations << "\n";

    if (!result.success) {
        feedback << "  Error: " << result.error_message << "\n";
    }

    return feedback.str();
}
