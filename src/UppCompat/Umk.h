#pragma once

#include "VfsShell.h"
#include "upp_builder.h"
#include "build_graph.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>

// Represents a U++ toolchain that normalizes compiler, linker, and build settings
struct UppToolchain {
    std::string compiler;
    std::string linker;
    std::vector<std::string> include_dirs;
    std::vector<std::string> library_dirs;
    std::map<std::string, std::string> flag_bundles; // debug, release, common, etc.
    
    // Constructor
    UppToolchain();
    
    // Initialize from a build method
    bool initFromBuildMethod(const UppBuildMethod& method, Vfs& vfs);
    
    // Get effective compile flags for a translation unit kind and build type
    std::vector<std::string> effectiveCompileFlags(const std::string& build_type) const;
    
    // Get effective link flags for a build type
    std::vector<std::string> effectiveLinkFlags(const std::string& build_type) const;
    
    // Discover source files in a package
    std::vector<std::string> discoverSources(const std::string& package_path) const;
    
private:
    // Helper to expand variables in flag strings
    std::string expandVariables(const std::string& flags, const std::map<std::string, std::string>& variables) const;
};

// Options for U++ build process
struct UppBuildOptions {
    std::string build_type = "debug"; // debug or release
    std::string output_dir;
    std::vector<std::string> extra_includes;
    bool verbose = false;
    bool dry_run = false;
};

// Summary of U++ build process
struct UppBuildSummary {
    BuildResult result;
    BuildGraph plan;
    std::vector<std::string> package_order;
    std::string builder_used;
};

// Main function to build a U++ workspace using the internal umk pipeline
UppBuildSummary build_upp_workspace(UppAssembly& assembly,
                                   Vfs& vfs,
                                   const UppBuildOptions& options);

// Helper function to generate internal U++ build command
std::string generate_internal_upp_build_command(const UppWorkspace& workspace,
                                              const UppPackage& pkg,
                                              const UppBuildOptions& options,
                                              Vfs& vfs,
                                              const UppBuildMethod* builder);