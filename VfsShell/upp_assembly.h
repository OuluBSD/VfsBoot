#ifndef _VfsShell_upp_assembly_h_
#define _VfsShell_upp_assembly_h_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "VfsShell.h"

// Structure to represent a U++ package
struct UppPackage {
    std::string name;
    std::string path;
    std::string description;                // Package description from .upp file
    std::vector<std::string> dependencies;  // List of dependencies (libraries, includes, other packages)
    std::vector<std::string> files;         // List of source files in the package
    bool is_primary;                        // Whether this is the primary package
    
    UppPackage(const std::string& n, const std::string& p, bool primary = false) 
        : name(n), path(p), is_primary(primary) {}
};

// Structure to represent the U++ workspace
struct UppWorkspace {
    std::string name;
    std::string assembly_path;              // Path to the .var file
    std::string base_dir;                   // Base directory for the workspace
    
    // Primary package (main package being developed)
    std::string primary_package;
    
    // All packages in the workspace (name -> package info)
    std::map<std::string, std::shared_ptr<UppPackage>> packages;
    
    // Constructor
    UppWorkspace(const std::string& n, const std::string& path) 
        : name(n), assembly_path(path) {}
    
    // Add a package to the workspace
    void add_package(std::shared_ptr<UppPackage> pkg);
    
    // Get a package by name
    std::shared_ptr<UppPackage> get_package(const std::string& name);
    
    // Get the primary package
    std::shared_ptr<UppPackage> get_primary_package();
    
    // Get all packages
    std::vector<std::shared_ptr<UppPackage>> get_all_packages();
};

// Class to parse and manage U++ assembly (.var) files
class UppAssembly {
private:
    std::shared_ptr<UppWorkspace> workspace;
    
public:
    UppAssembly();
    
    // Load a .var file from the given path
    bool load_from_file(const std::string& path);
    
    // Load from content string
    bool load_from_content(const std::string& content, const std::string& path = "");
    
    // Set the workspace directly
    void set_workspace(std::shared_ptr<UppWorkspace> ws);
    
    // Get the workspace
    std::shared_ptr<UppWorkspace> get_workspace();
    
    // Get the primary package
    std::shared_ptr<UppPackage> get_primary_package();
    
    // Get all packages
    std::vector<std::shared_ptr<UppPackage>> get_all_packages();
    
    // Save the assembly to a .var file
    bool save_to_file(const std::string& path);
    
    // Get the assembly content as string
    std::string to_string();
    
    // Parse .var file content
    bool parse_var_content(const std::string& content, const std::string& path = "");
    
    // Build the project using umk
    bool build_project(const std::string& build_type = "debug", const std::string& output_path = "");
    
    // Detect packages by scanning directories for .upp files
    bool detect_packages_from_directory(Vfs& vfs, const std::string& base_path);
    
    // Parse U++ package file (.upp format)
    bool parse_upp_file_content(const std::string& content, UppPackage& pkg);
    
    // Generate VFS structure for the workspace
    void create_vfs_structure(Vfs& vfs, size_t overlay_id = 0);
};

// U++ Assembly GUI structure
struct UppAssemblyGui {
    // Window handles for different panes
    void* main_editor;      // Main code editor area
    void* package_list;     // Package list at top left
    void* file_list;        // File list at bottom left
    void* menu_bar;         // Menu bar at top
    
    // State
    std::shared_ptr<UppAssembly> assembly;
    std::shared_ptr<UppWorkspace> workspace;
    
    // Selection tracking
    int selected_package_idx;
    int selected_file_idx;
    int selected_line;
    int editor_offset;
    
    UppAssemblyGui(std::shared_ptr<UppAssembly> asm_ptr);
    
    // Initialize the GUI
    void init();
    
    // Run the GUI main loop
    void run();
    
    // Draw the GUI
    void draw();
    
    // Helper drawing functions
    void draw_panel(int start_row, int start_col, int height, int width, const std::string& title);
    void display_packages(int start_row, int start_col, int height, int width);
    void display_files(int start_row, int start_col, int height, int width);
    void display_editor_content(int start_row, int start_col, int height, int width);
    void draw_status_bar(int row, int col, int width);
    
    // Handle input
    int handle_input();
    
    // Update package list
    void update_package_list();
    
    // Update file list
    void update_file_list();
    
    // Update editor content
    void update_editor();
    
    // Show help
    void show_help();
    
    // Cleanup
    void cleanup();
};

#endif // _VfsShell_upp_assembly_h_