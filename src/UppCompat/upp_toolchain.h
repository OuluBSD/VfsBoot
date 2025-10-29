#ifndef _UppCompat_upp_toolchain_h_
#define _UppCompat_upp_toolchain_h_

#include "../VfsShell/VfsShell.h"
#include "upp_builder.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

// Forward declarations
struct Vfs;

// UppToolchain represents a U++ toolchain configuration
struct UppToolchain {
    std::string id;
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

// UppToolchainRegistry manages available toolchains
struct UppToolchainRegistry {
    std::map<std::string, std::shared_ptr<UppToolchain>> toolchains;
    std::string active_toolchain_id;
    
    // Add a toolchain
    void add(const std::shared_ptr<UppToolchain>& toolchain);
    
    // Get a toolchain by ID
    const UppToolchain* get(const std::string& id) const;
    
    // Get the active toolchain
    const UppToolchain* active() const;
    
    // Set the active toolchain
    void setActive(const std::string& id);
    
    // List all toolchain IDs
    std::vector<std::string> list() const;
    
    // Check if a toolchain exists
    bool has(const std::string& id) const;
};

// Global toolchain registry
extern UppToolchainRegistry g_toolchain_registry;

#endif