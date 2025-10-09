#include "scenario.h"
#include "runner.h"
#include "../VfsShell/codex.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <apr_general.h>

namespace fs = std::filesystem;

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <scenario-directory>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -v, --verbose     Enable verbose output\n";
    std::cout << "  -i, --iterations  Max breakdown iterations per scenario (default: 10)\n";
    std::cout << "  -o, --output      Output file for training data (default: training_data.json)\n";
    std::cout << "  -h, --help        Show this help message\n";
    std::cout << "\nDescription:\n";
    std::cout << "  Runs all .scenario files in the specified directory and generates\n";
    std::cout << "  training data from successful and failed breakdown attempts.\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << prog << " -v -o train.json scenarios/\n";
}

struct TrainingData {
    std::string scenario_name;
    std::string user_intent;
    std::string generated_plan;
    std::vector<std::string> actions_taken;
    bool success;
    size_t iterations;
    std::string error_message;

    // Additional metrics from PlannerMetrics
    double execution_time_ms;
    size_t vfs_nodes_examined;
    std::vector<std::string> rules_triggered;
    std::vector<std::string> rules_failed;
};

std::vector<std::string> find_scenario_files(const std::string& dir) {
    std::vector<std::string> files;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".scenario") {
            files.push_back(entry.path().string());
        }
    }

    return files;
}

int main(int argc, char* argv[]) {
    // Initialize APR (required for BinaryDiff/ScopeStore)
    apr_status_t status = apr_initialize();
    if (status != APR_SUCCESS) {
        char errbuf[256];
        apr_strerror(status, errbuf, sizeof(errbuf));
        std::cerr << "Error: Failed to initialize APR: " << errbuf << "\n";
        return 1;
    }
    // Register cleanup
    atexit(apr_terminate);

    bool verbose = false;
    size_t max_iterations = 10;
    std::string output_file = "training_data.json";
    std::string scenario_dir;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 < argc) {
                max_iterations = std::stoul(argv[++i]);
            } else {
                std::cerr << "Error: -i requires an argument\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                output_file = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            scenario_dir = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (scenario_dir.empty()) {
        std::cerr << "Error: No scenario directory specified\n";
        print_usage(argv[0]);
        return 1;
    }

    // Find all scenario files
    std::vector<std::string> scenario_files = find_scenario_files(scenario_dir);

    if (scenario_files.empty()) {
        std::cerr << "Error: No .scenario files found in " << scenario_dir << "\n";
        return 1;
    }

    std::cout << "Found " << scenario_files.size() << " scenario file(s)\n";

    // Initialize VFS and scope store (shared across scenarios)
    Vfs vfs;
    ScopeStore scope_store;

    // Initialize global metrics collector to aggregate across all scenarios
    MetricsCollector global_metrics;

    std::vector<TrainingData> training_results;
    size_t passed = 0, failed = 0;

    // Process each scenario
    for (const auto& file : scenario_files) {
        std::cout << "\n=== Processing: " << file << " ===\n";

        std::ifstream input(file);
        if (!input) {
            std::cerr << "Warning: Cannot open file: " << file << "\n";
            continue;
        }

        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());

        harness::Scenario scenario;
        try {
            scenario = harness::Scenario::parse(content);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to parse " << file << ": " << e.what() << "\n";
            continue;
        }

        // Create runner with metrics
        harness::ScenarioRunner runner(vfs, scope_store, &global_metrics);
        runner.setVerbose(verbose);

        // Run breakdown loop with metrics
        harness::BreakdownLoop loop(runner, scope_store, &global_metrics);
        loop.setMaxIterations(max_iterations);

        harness::BreakdownResult result = loop.run(scenario);

        // Collect training data with metrics
        TrainingData data;
        data.scenario_name = scenario.name;
        data.user_intent = scenario.user_intent;
        data.generated_plan = scenario.expected_plan;  // Would be actual plan in full impl
        data.actions_taken = scenario.expected_actions;
        data.success = result.success;
        data.iterations = result.iterations;
        data.error_message = result.error_message;

        // Copy metrics from the last run
        if (!global_metrics.history.empty()) {
            const auto& last_metrics = global_metrics.history.back();
            data.execution_time_ms = last_metrics.execution_time_ms;
            data.vfs_nodes_examined = last_metrics.vfs_nodes_examined;
            data.rules_triggered = last_metrics.rules_triggered;
            data.rules_failed = last_metrics.rules_failed;
        } else {
            data.execution_time_ms = 0.0;
            data.vfs_nodes_examined = 0;
        }

        training_results.push_back(data);

        if (result.success) {
            passed++;
            std::cout << "✓ Passed\n";
        } else {
            failed++;
            std::cout << "✗ Failed: " << result.error_message << "\n";
        }

        // Reset VFS for next scenario
        vfs = Vfs();
    }

    // Write training data to output file
    std::cout << "\n=== Training Summary ===\n";
    std::cout << "Total scenarios: " << training_results.size() << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";

    // Display global metrics summary
    if (!global_metrics.history.empty()) {
        auto top_triggered = global_metrics.getMostTriggeredRules(10);
        auto top_failed = global_metrics.getMostFailedRules(10);
        double avg_success = global_metrics.getAverageSuccessRate();
        double avg_iterations = global_metrics.getAverageIterations();

        std::cout << "\n=== Aggregated Metrics ===\n";
        std::cout << "Average Success Rate: " << (avg_success * 100.0) << "%\n";
        std::cout << "Average Iterations: " << avg_iterations << "\n";

        if (!top_triggered.empty()) {
            std::cout << "\nTop 10 Triggered Rules:\n";
            for (const auto& rule : top_triggered) {
                std::cout << "  - " << rule << "\n";
            }
        }

        if (!top_failed.empty()) {
            std::cout << "\nTop 10 Failed Rules:\n";
            for (const auto& rule : top_failed) {
                std::cout << "  - " << rule << "\n";
            }
        }
    }

    std::ofstream output(output_file);
    if (!output) {
        std::cerr << "Error: Cannot write to " << output_file << "\n";
        return 1;
    }

    // Helper function to escape JSON strings
    auto escape_json = [](const std::string& s) -> std::string {
        std::string result;
        for (char c : s) {
            if (c == '"' || c == '\\') result += '\\';
            else if (c == '\n') { result += "\\n"; continue; }
            else if (c == '\r') { result += "\\r"; continue; }
            else if (c == '\t') { result += "\\t"; continue; }
            result += c;
        }
        return result;
    };

    // Write comprehensive JSON format with metrics
    output << "{\n";
    output << "  \"summary\": {\n";
    output << "    \"total_scenarios\": " << training_results.size() << ",\n";
    output << "    \"passed\": " << passed << ",\n";
    output << "    \"failed\": " << failed << ",\n";
    output << "    \"success_rate\": " << (training_results.empty() ? 0.0 : (double)passed / training_results.size()) << "\n";
    output << "  },\n";
    output << "  \"training_data\": [\n";

    for (size_t i = 0; i < training_results.size(); ++i) {
        const auto& data = training_results[i];
        output << "    {\n";
        output << "      \"scenario_name\": \"" << escape_json(data.scenario_name) << "\",\n";
        output << "      \"user_intent\": \"" << escape_json(data.user_intent) << "\",\n";
        output << "      \"generated_plan\": \"" << escape_json(data.generated_plan) << "\",\n";
        output << "      \"success\": " << (data.success ? "true" : "false") << ",\n";
        output << "      \"iterations\": " << data.iterations << ",\n";
        output << "      \"execution_time_ms\": " << data.execution_time_ms << ",\n";
        output << "      \"vfs_nodes_examined\": " << data.vfs_nodes_examined << ",\n";
        output << "      \"rules_triggered\": [";
        for (size_t j = 0; j < data.rules_triggered.size(); ++j) {
            output << "\"" << escape_json(data.rules_triggered[j]) << "\"";
            if (j + 1 < data.rules_triggered.size()) output << ", ";
        }
        output << "],\n";
        output << "      \"rules_failed\": [";
        for (size_t j = 0; j < data.rules_failed.size(); ++j) {
            output << "\"" << escape_json(data.rules_failed[j]) << "\"";
            if (j + 1 < data.rules_failed.size()) output << ", ";
        }
        output << "],\n";
        output << "      \"error_message\": \"" << escape_json(data.error_message) << "\"\n";
        output << "    }";
        if (i + 1 < training_results.size()) {
            output << ",";
        }
        output << "\n";
    }

    output << "  ]\n";
    output << "}\n";

    std::cout << "\nTraining data with metrics written to: " << output_file << "\n";

    return (failed == 0) ? 0 : 1;
}
