#pragma once

#include "upp_builder.h"
#include "VfsShell.h"

#include <string>
#include <vector>
#include <map>

// Forward declarations
class Vfs;
struct UppBuildMethod;
struct UppWorkspace;
struct UppPackage;

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
    
    // Get effective compile flags for a build type
    std::vector<std::string> effectiveCompileFlags(const std::string& build_type) const;
    
    // Get effective link flags for a build type
    std::vector<std::string> effectiveLinkFlags(const std::string& build_type) const;
    
    // Discover source files in a package
    std::vector<std::string> discoverSources(const std::string& package_path) const;
    
private:
    // Helper to expand variables in flag strings
    std::string expandVariables(const std::string& flags, const std::map<std::string, std::string>& variables) const;
};