#ifndef _UppCompat_upp_workspace_build_h_
#define _UppCompat_upp_workspace_build_h_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// Forward declarations
struct Vfs;
struct UppWorkspace;
struct UppPackage;
struct UppBuildMethod;

// WorkspaceBuildOptions represents build configuration for a workspace
struct WorkspaceBuildOptions {
    std::string target_package;
    std::string build_type = "debug";  // debug or release
    std::string output_dir;
    std::string builder_name;
    std::vector<std::string> extra_includes;
    bool verbose = false;
    bool dry_run = false;
    int max_jobs = 1;
};

// UppWorkspaceBuild represents a U++ workspace build configuration
struct UppWorkspaceBuild {
    std::string workspace_path;
    std::string build_type = "debug";  // debug or release
    std::string target_package;
    std::string output_dir;
    std::vector<std::string> extra_includes;
    bool verbose = false;
    bool dry_run = false;
    int max_jobs = 1;  // Number of parallel jobs
    
    // Load from VFS
    bool load_from_vfs(Vfs& vfs, const std::string& vfs_path);
    
    // Save to VFS
    bool save_to_vfs(Vfs& vfs, const std::string& vfs_path);
    
    // Get effective command-line arguments for umk
    std::vector<std::string> get_umk_args() const;
    
    // Execute the build
    bool execute(Vfs& vfs) const;
};

// UppBuildResult represents the result of a U++ build
struct UppBuildResult {
    bool success = false;
    std::string output;
    std::string error;
    long duration_ms = 0;
    std::vector<std::string> built_files;
    
    // Clear the result
    void clear();
    
    // Merge with another result
    void merge(const UppBuildResult& other);
};

// Parse build options from command-line arguments
UppWorkspaceBuild parse_build_options(const std::vector<std::string>& args);

// Show help for build options
void show_build_help();

#endif