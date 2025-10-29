#ifndef _UppCompat_BuildGraph_h_
#define _UppCompat_BuildGraph_h_

// All includes have been moved to UppCompat.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from VfsCore.h
// #include "../VfsCore/VfsCore.h"  // Commented for U++ convention - included in main header

// Forward declarations
struct Vfs;

// BuildResult holds the result of a build operation
struct BuildResult {
    bool success = false;
    String output;
    String error;
    long duration_ms = 0;
    typedef BuildResult CLASSNAME;  // Required for THISBACK macros if used
};

// BuildCommand represents a command to execute as part of a build
struct BuildCommand {
    enum class Type {
        Shell,
        Copy,
        Custom
    };
    
    Type type = Type::Shell;
    String text;  // Command text for shell commands
    std::function<bool(BuildResult&)> executor;  // For custom commands
    
    BuildCommand() = default;
    explicit BuildCommand(const String& cmd) : type(Type::Shell), text(cmd) {}
    typedef BuildCommand CLASSNAME;  // Required for THISBACK macros if used
};

// BuildRule defines a build rule with dependencies and commands
struct BuildRule {
    String name;
    Vector<String> dependencies;
    Vector<BuildCommand> commands;
    Vector<String> outputs;
    bool always_run = false;  // If true, always execute this rule
    
    BuildRule() = default;
    explicit BuildRule(const String& n) : name(n) {}
    typedef BuildRule CLASSNAME;  // Required for THISBACK macros if used
};

// BuildOptions contains options for the build process
struct BuildOptions {
    bool verbose = false;
    std::function<bool(const BuildRule&, BuildResult&, bool verbose)> executor = nullptr; // Custom executor
    String log_file;  // Optional log file path
    typedef BuildOptions CLASSNAME;  // Required for THISBACK macros if used
};

// BuildGraph manages the dependency graph for builds
struct BuildGraph {
    Map<String, BuildRule> rules;
    
    BuildResult build(const String& target, Vfs& vfs, const BuildOptions& options = BuildOptions{});
    
    static bool runShellCommands(const BuildRule& rule, BuildResult& result, bool verbose = false);
    typedef BuildGraph CLASSNAME;  // Required for THISBACK macros if used
};

#endif