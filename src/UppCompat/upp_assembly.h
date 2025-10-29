#ifndef _UppCompat_upp_assembly_h_
#define _UppCompat_upp_assembly_h_

#include "../VfsShell/VfsShell.h"

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include <numeric>

// Forward declarations
struct Vfs;

// UppPackage represents a U++ package
struct UppPackage {
    std::string name;
    std::string path;
    std::string description;
    std::vector<std::string> files;
    std::vector<std::string> dependencies;
    bool is_primary = false;

    UppPackage(const std::string& n, const std::string& p, bool primary = false) 
        : name(n), path(p), is_primary(primary) {}
};

// UppWorkspace represents a collection of U++ packages
struct UppWorkspace {
    std::string name;
    std::string assembly_path;
    std::string base_dir;
    std::map<std::string, std::shared_ptr<UppPackage>> packages;
    std::string primary_package;

    UppWorkspace(const std::string& n, const std::string& path) 
        : name(n), assembly_path(path) {}

    void add_package(std::shared_ptr<UppPackage> pkg);
    std::shared_ptr<UppPackage> get_package(const std::string& name);
    std::shared_ptr<UppPackage> get_primary_package();
    std::vector<std::shared_ptr<UppPackage>> get_all_packages();
};

// UppAssembly handles loading, parsing, and managing U++ assemblies
class UppAssembly {
private:
    std::shared_ptr<UppWorkspace> workspace;

public:
    UppAssembly();
    
    // File operations
    bool load_from_file(const std::string& path);
    bool load_from_content(const std::string& content, const std::string& path);
    bool save_to_file(const std::string& path);
    std::string to_string();

    // Workspace management
    void set_workspace(std::shared_ptr<UppWorkspace> ws);
    std::shared_ptr<UppWorkspace> get_workspace();
    std::shared_ptr<UppPackage> get_primary_package();
    std::vector<std::shared_ptr<UppPackage>> get_all_packages();

    // Package detection and parsing
    bool detect_packages_from_directory(Vfs& vfs, const std::string& base_path, bool verbose = false);
    bool parse_upp_file_content(const std::string& content, UppPackage& pkg);
    bool parse_var_content(const std::string& content, const std::string& path);

    // Build operations
    bool build_project(const std::string& build_type = "debug", const std::string& output_path = "");

    // VFS operations
    void create_vfs_structure(Vfs& vfs, size_t overlay_id = 0);
};

// UppAssemblyGui provides a text-based UI for managing U++ assemblies
class UppAssemblyGui {
private:
    std::shared_ptr<UppAssembly> assembly;
    std::shared_ptr<UppWorkspace> workspace;

    // UI components
    void* main_editor;
    void* package_list;
    void* file_list;
    void* menu_bar;

    // Selection state
    int selected_package_idx;
    int selected_file_idx;
    int selected_line;
    int editor_offset;

    // UI drawing methods
    void draw_panel(int start_row, int start_col, int height, int width, const std::string& title);
    void display_packages(int start_row, int start_col, int height, int width);
    void display_files(int start_row, int start_col, int height, int width);
    void display_editor_content(int start_row, int start_col, int height, int width);
    void draw_status_bar(int row, int col, int width);

public:
    UppAssemblyGui(std::shared_ptr<UppAssembly> asm_ptr);

    void init();
    void run();
    void draw();
    int handle_input();
    void show_help();
    void cleanup();

    // UI update methods
    void update_package_list();
    void update_file_list();
    void update_editor();
};

// Utility functions
inline std::string trim_copy(const std::string& s) {
    auto start = s.begin();
    auto end = s.end();
    while (start != end && std::isspace(*start)) start++;
    if (start == end) return std::string();
    do { end--; } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

#endif