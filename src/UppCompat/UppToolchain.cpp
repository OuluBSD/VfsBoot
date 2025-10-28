#include "upp_toolchain.h"
#include "VfsShell.h"
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

UppToolchain::UppToolchain() 
    : compiler("c++"), linker("c++") {
}

bool UppToolchain::initFromBuildMethod(const UppBuildMethod& method, Vfs& vfs) {
    // Get compiler
    auto compiler_opt = method.get("COMPILER");
    if (compiler_opt) {
        compiler = *compiler_opt;
    }
    
    // Get linker
    auto linker_opt = method.get("LINKER");
    if (linker_opt) {
        linker = *linker_opt;
    } else {
        linker = compiler; // Default to compiler if no linker specified
    }
    
    // Get include directories
    auto includes_opt = method.get("INCLUDES");
    if (includes_opt) {
        auto include_list = method.splitList("INCLUDES", ';');
        for (const auto& inc : include_list) {
            if (!inc.empty()) {
                include_dirs.push_back(inc);
            }
        }
    }
    
    // Get library directories
    auto libs_opt = method.get("LIBS");
    if (libs_opt) {
        auto lib_list = method.splitList("LIBS", ';');
        for (const auto& lib : lib_list) {
            if (!lib.empty()) {
                library_dirs.push_back(lib);
            }
        }
    }
    
    // Get flag bundles
    std::vector<std::string> flag_keys = {"COMMON_OPTIONS", "DEBUG_OPTIONS", "RELEASE_OPTIONS", 
                                          "GUI_OPTIONS", "USEMALLOC_OPTIONS"};
    for (const auto& key : flag_keys) {
        auto flags_opt = method.get(key);
        if (flags_opt) {
            flag_bundles[key] = *flags_opt;
        }
    }
    
    return true;
}

std::vector<std::string> UppToolchain::effectiveCompileFlags(const std::string& build_type) const {
    std::vector<std::string> flags;
    
    // Add common options first
    auto common_it = flag_bundles.find("COMMON_OPTIONS");
    if (common_it != flag_bundles.end()) {
        flags.push_back(common_it->second);
    }
    
    // Add build-type specific options
    std::string type_key = (build_type == "release") ? "RELEASE_OPTIONS" : "DEBUG_OPTIONS";
    auto type_it = flag_bundles.find(type_key);
    if (type_it != flag_bundles.end()) {
        flags.push_back(type_it->second);
    }
    
    // Add GUI options if needed
    auto gui_it = flag_bundles.find("GUI_OPTIONS");
    if (gui_it != flag_bundles.end()) {
        flags.push_back(gui_it->second);
    }
    
    // Add USEMALLOC options if present
    auto malloc_it = flag_bundles.find("USEMALLOC_OPTIONS");
    if (malloc_it != flag_bundles.end()) {
        flags.push_back(malloc_it->second);
    }
    
    return flags;
}

std::vector<std::string> UppToolchain::effectiveLinkFlags(const std::string& build_type) const {
    // For now, link flags are the same as compile flags
    return effectiveCompileFlags(build_type);
}

std::vector<std::string> UppToolchain::discoverSources(const std::string& package_path) const {
    std::vector<std::string> sources;
    
    // This is a simplified version that just returns an empty vector
    // In a full implementation, we would scan the package directory for source files
    return sources;
}

std::string UppToolchain::expandVariables(const std::string& flags, 
                                         const std::map<std::string, std::string>& variables) const {
    std::string result = flags;
    
    // Simple variable expansion - replace ${VAR} or $(VAR) with values
    for (const auto& [var_name, var_value] : variables) {
        std::string var_pattern1 = "${" + var_name + "}";
        std::string var_pattern2 = "$(" + var_name + ")";
        
        size_t pos = 0;
        while ((pos = result.find(var_pattern1, pos)) != std::string::npos) {
            result.replace(pos, var_pattern1.length(), var_value);
            pos += var_value.length();
        }
        
        pos = 0;
        while ((pos = result.find(var_pattern2, pos)) != std::string::npos) {
            result.replace(pos, var_pattern2.length(), var_value);
            pos += var_value.length();
        }
    }
    
    return result;
}