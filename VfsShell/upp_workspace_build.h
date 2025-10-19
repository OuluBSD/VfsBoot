#pragma once

#include "build_graph.h"

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
};

WorkspaceBuildSummary build_workspace(UppAssembly& assembly,
                                      Vfs& vfs,
                                      const WorkspaceBuildOptions& options);
