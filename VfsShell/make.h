#pragma once



// ============================================================================
// Minimal GNU Make Implementation
// ============================================================================

// Makefile rule representation
struct MakeRule {
    std::string target;                      // Target name
    std::vector<std::string> dependencies;   // Prerequisites
    std::vector<std::string> commands;       // Shell commands to execute
    bool is_phony;                           // .PHONY target (always rebuild)

    MakeRule() : is_phony(false) {}
    MakeRule(std::string t) : target(std::move(t)), is_phony(false) {}
};

// Makefile parser and executor
struct MakeFile {
    std::map<std::string, std::string> variables;  // Variable definitions
    std::map<std::string, MakeRule> rules;         // All rules indexed by target
    std::set<std::string> phony_targets;           // .PHONY targets

    // Parse Makefile content from string
    void parse(const std::string& content);

    // Variable substitution $(VAR) and ${VAR}
    std::string expandVariables(const std::string& text) const;

    // Expand automatic variables ($@, $<, $^)
    std::string expandAutomaticVars(const std::string& text, const MakeRule& rule) const;

    // Check if target needs rebuild (timestamp comparison)
    bool needsRebuild(const std::string& target, Vfs& vfs) const;

    // Get file modification time from VFS or filesystem
    std::optional<uint64_t> getModTime(const std::string& path, Vfs& vfs) const;

    // Build dependency graph and execute rules
    struct BuildResult {
        bool success;
        std::string output;
        std::vector<std::string> targets_built;
        std::vector<std::string> errors;
    };

    BuildResult build(const std::string& target, Vfs& vfs, bool verbose = false);

private:
    // Recursive build with cycle detection
    bool buildTarget(const std::string& target, Vfs& vfs,
                    std::set<std::string>& building,  // Cycle detection
                    std::set<std::string>& built,     // Already built
                    BuildResult& result, bool verbose);

    // Execute shell commands for a rule
    bool executeCommands(const MakeRule& rule, BuildResult& result, bool verbose);

    // Parse a single line
    void parseLine(const std::string& line, std::string& current_target);
};
