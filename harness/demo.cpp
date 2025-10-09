#include "scenario.h"
#include "runner.h"
#include "../VfsShell/codex.h"
#include <iostream>
#include <fstream>
#include <cstring>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <scenario-file>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -v, --verbose     Enable verbose output\n";
    std::cout << "  -i, --iterations  Max breakdown iterations (default: 10)\n";
    std::cout << "  -h, --help        Show this help message\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << prog << " -v scenarios/basic/hello-world.scenario\n";
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    size_t max_iterations = 10;
    std::string scenario_file;

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
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argv[i][0] != '-') {
            scenario_file = argv[i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (scenario_file.empty()) {
        std::cerr << "Error: No scenario file specified\n";
        print_usage(argv[0]);
        return 1;
    }

    // Load scenario
    std::ifstream input(scenario_file);
    if (!input) {
        std::cerr << "Error: Cannot open scenario file: " << scenario_file << "\n";
        return 1;
    }

    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());

    harness::Scenario scenario;
    try {
        scenario = harness::Scenario::parse(content);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing scenario: " << e.what() << "\n";
        return 1;
    }

    if (verbose) {
        std::cout << "Loaded scenario: " << scenario.name << "\n";
        std::cout << "Description: " << scenario.description << "\n\n";
    }

    // Initialize VFS and scope store
    Vfs vfs;
    ScopeStore scope_store;

    // Create runner
    harness::ScenarioRunner runner(vfs, scope_store);
    runner.setVerbose(verbose);

    // Run breakdown loop
    harness::BreakdownLoop loop(runner, scope_store);
    loop.setMaxIterations(max_iterations);

    std::cout << "Starting breakdown loop with " << max_iterations << " max iterations...\n\n";

    harness::BreakdownResult result = loop.run(scenario);

    // Print feedback
    std::string feedback = loop.generateFeedback(result);
    std::cout << "\n" << feedback << "\n";

    return result.success ? 0 : 1;
}
