#pragma once

#include "../VfsShell/VfsShell.h"
#include "../VfsShell/BuildGraph.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

class Vfs;
class UppAssembly;

struct WorkspaceBuildOptions {
    std::string build_type = "debug";
    std::string builder_name;
    std::string output_dir;
    std::vector<std::string> extra_includes;
    std::string target_package;
    bool verbose = false;
    bool dry_run = false;
};

struct WorkspaceBuildSummary {
    BuildResult result;
    BuildGraph plan;
    std::vector<std::string> package_order;
    std::string builder_used;
    std::map<std::string, std::string> package_outputs;  // package_name -> output_path (host filesystem)
};

WorkspaceBuildSummary build_workspace(UppAssembly& assembly,
                                      Vfs& vfs,
                                      const WorkspaceBuildOptions& options);
