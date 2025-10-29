#ifndef _UppCompat_Umk_h_
#define _UppCompat_Umk_h_

// All includes have been moved to UppCompat.h - the main header
// This header is not self-contained as per U++ convention
// For reference: This header needs types from VfsShell.h, UppBuilder.h, BuildGraph.h, and upp_toolchain.h
// #include "../VfsShell/VfsShell.h"        // Commented for U++ convention - included in main header
// #include "../UppCompat/UppBuilder.h"     // Commented for U++ convention - included in main header
// #include "../VfsShell/BuildGraph.h"      // Commented for U++ convention - included in main header
// #include "upp_toolchain.h"               // Commented for U++ convention - included in main header

// Forward declarations
struct Vfs;

// Options for U++ build process
struct UppBuildOptions {
    String build_type = "debug"; // debug or release
    String output_dir;
    Vector<String> extra_includes;
    bool verbose = false;
    bool dry_run = false;
    String target_package;  // Specific package to build (empty = primary)
    String builder_name;    // Specific builder to use (empty = active)
    typedef UppBuildOptions CLASSNAME;  // Required for THISBACK macros if used
};

// Summary of U++ build process
struct UppBuildSummary {
    BuildResult result;
    BuildGraph plan;
    Vector<String> package_order;
    String builder_used;
    typedef UppBuildSummary CLASSNAME;  // Required for THISBACK macros if used
};

// Main function to build a U++ workspace using the internal umk pipeline
UppBuildSummary build_upp_workspace(UppAssembly& assembly,
                                   Vfs& vfs,
                                   const UppBuildOptions& options);

#endif
