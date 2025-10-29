#ifndef _UppCompat_Umk_h_
#define _UppCompat_Umk_h_

#include "../VfsShell/VfsShell.h"
#include "../UppCompat/UppBuilder.h"
#include "../VfsShell/BuildGraph.h"
#include "upp_toolchain.h"

#include <string>
#include <vector>
#include <memory>
#include <optional>

// Forward declarations
struct Vfs;


// Options for U++ build process
struct UppBuildOptions {
    std::string build_type = "debug"; // debug or release
    std::string output_dir;
    std::vector<std::string> extra_includes;
    bool verbose = false;
    bool dry_run = false;
    std::string target_package;  // Specific package to build (empty = primary)
    std::string builder_name;    // Specific builder to use (empty = active)
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
std::string generate_internal_upp_build_command_impl(const UppWorkspace& workspace,
                                              const UppPackage& pkg,
                                              const UppBuildOptions& options,
                                              Vfs& vfs,
                                              const UppBuildMethod* builder);

#endif
