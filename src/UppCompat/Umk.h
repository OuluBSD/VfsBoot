#ifndef _UppCompat_Umk_h_
#define _UppCompat_Umk_h_

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

// Forward declarations
struct Vfs;
struct UppWorkspace;
struct UppPackage;
struct UppBuildMethod;
struct UppAssembly;
struct BuildResult;
class BuildGraph;

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
    std::map<std::string, std::string> package_outputs;
};

// Main function to build a U++ workspace using the internal umk pipeline
UppBuildSummary build_upp_workspace(UppAssembly& assembly,
                                   Vfs& vfs,
                                   const UppBuildOptions& options);

// Helper functions
std::string prefer_host_path(Vfs& vfs, const std::string& path);
std::string g_shell_quote(const std::string& value);
std::string package_target(const std::string& name);

void collect_packages(const std::shared_ptr<UppWorkspace>& workspace,
                     const std::string& pkg_name,
                     std::unordered_set<std::string>& visiting,
                     std::unordered_set<std::string>& visited,
                     std::vector<std::string>& order);

std::vector<std::string> build_asmlist(const UppWorkspace& workspace,
                                       const UppPackage& pkg,
                                       const UppBuildOptions& options,
                                       Vfs& vfs,
                                       const UppBuildMethod* builder);

std::string umk_flags(const UppBuildOptions& options, bool verbose);

std::string default_output_path(const UppWorkspace& workspace,
                                const UppPackage& pkg,
                                const UppBuildOptions& options,
                                Vfs& vfs);

std::string render_command_template(const std::string& tpl,
                                    const std::map<std::string, std::string>& vars);

std::string generate_internal_upp_build_command_impl(const UppWorkspace& workspace,
                                                     const UppPackage& pkg,
                                                     const UppBuildOptions& options,
                                                     Vfs& vfs,
                                                     const UppBuildMethod* builder);

#endif
