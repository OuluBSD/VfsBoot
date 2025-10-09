#include "scenario.h"
#include "runner.h"
#include "../VfsShell/codex.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstring>

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

        // Create runner
        harness::ScenarioRunner runner(vfs, scope_store);
        runner.setVerbose(verbose);

        // Run breakdown loop
        harness::BreakdownLoop loop(runner, scope_store);
        loop.setMaxIterations(max_iterations);

        harness::BreakdownResult result = loop.run(scenario);

        // Collect training data
        TrainingData data;
        data.scenario_name = scenario.name;
        data.user_intent = scenario.user_intent;
        data.generated_plan = scenario.expected_plan;  // Would be actual plan in full impl
        data.actions_taken = scenario.expected_actions;
        data.success = result.success;
        data.iterations = result.iterations;
        data.error_message = result.error_message;

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

    std::ofstream output(output_file);
    if (!output) {
        std::cerr << "Error: Cannot write to " << output_file << "\n";
        return 1;
    }

    // Write simple JSON format
    output << "{\n";
    output << "  \"training_data\": [\n";

    for (size_t i = 0; i < training_results.size(); ++i) {
        const auto& data = training_results[i];
        output << "    {\n";
        output << "      \"scenario_name\": \"" << data.scenario_name << "\",\n";
        output << "      \"user_intent\": \"" << data.user_intent << "\",\n";
        output << "      \"generated_plan\": \"" << data.generated_plan << "\",\n";
        output << "      \"success\": " << (data.success ? "true" : "false") << ",\n";
        output << "      \"iterations\": " << data.iterations << ",\n";
        output << "      \"error_message\": \"" << data.error_message << "\"\n";
        output << "    }";
        if (i + 1 < training_results.size()) {
            output << ",";
        }
        output << "\n";
    }

    output << "  ]\n";
    output << "}\n";

    std::cout << "\nTraining data written to: " << output_file << "\n";

    return (failed == 0) ? 0 : 1;
}
