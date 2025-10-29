#ifndef _UppCompat_build_graph_h_
#define _UppCompat_build_graph_h_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <iostream>

// Forward declarations
struct Vfs;

// BuildResult holds the result of a build operation
struct BuildResult {
    bool success = false;
    std::string output;
    std::string error;
    long duration_ms = 0;
};

// BuildCommand represents a command to execute as part of a build
struct BuildCommand {
    enum class Type {
        Shell,
        Copy,
        Custom
    };
    
    Type type = Type::Shell;
    std::string text;  // Command text for shell commands
    std::function<bool(BuildResult&)> executor;  // For custom commands
    
    BuildCommand() = default;
    explicit BuildCommand(const std::string& cmd) : type(Type::Shell), text(cmd) {}
};

// BuildRule defines a build rule with dependencies and commands
struct BuildRule {
    std::string name;
    std::vector<std::string> dependencies;
    std::vector<BuildCommand> commands;
    std::vector<std::string> outputs;
    bool always_run = false;  // If true, always execute this rule
    
    BuildRule() = default;
    explicit BuildRule(const std::string& n) : name(n) {}
};

// BuildOptions contains options for the build process
struct BuildOptions {
    bool verbose = false;
    std::function<bool(const BuildRule&, BuildResult&, bool verbose)> executor = nullptr; // Custom executor
    std::string log_file;  // Optional log file path
};

// BuildGraph manages the dependency graph for builds
struct BuildGraph {
    std::map<std::string, BuildRule> rules;
    
    BuildResult build(const std::string& target, Vfs& vfs, const BuildOptions& options = BuildOptions{});
    
    static bool runShellCommands(const BuildRule& rule, BuildResult& result, bool verbose = false);
};

#endif